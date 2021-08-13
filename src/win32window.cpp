#include "win32window.h"
#include "nvim/nvim.h"

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                LPARAM lparam)
{
    if (msg == WM_CREATE)
    {
        LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
        SetWindowLongPtr(
            hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
        return 0;
    }

    auto w =
        reinterpret_cast<Win32Window *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    return w->Proc(hwnd, msg, wparam, lparam);
}

Win32Window::Win32Window(HINSTANCE instance)
{
    _instance = instance;
}

Win32Window::~Win32Window()
{
    DestroyWindow(_hwnd);
    UnregisterClass(_className.c_str(), _instance);
}

HWND Win32Window::Create(const wchar_t *window_class_name,
                         const wchar_t *window_title)
{
    WNDCLASSEX window_class{.cbSize = sizeof(WNDCLASSEX),
                            .style = CS_HREDRAW | CS_VREDRAW,
                            .lpfnWndProc = &WndProc,
                            .hInstance = _instance,
                            .hIcon = static_cast<HICON>(LoadImage(
                                GetModuleHandle(NULL), L"NVIM_ICON", IMAGE_ICON,
                                LR_DEFAULTSIZE, LR_DEFAULTSIZE, 0)),
                            .hCursor = LoadCursor(NULL, IDC_ARROW),
                            .hbrBackground = nullptr,
                            .lpszClassName = window_class_name,
                            .hIconSm = static_cast<HICON>(LoadImage(
                                GetModuleHandle(NULL), L"NVIM_ICON", IMAGE_ICON,
                                LR_DEFAULTSIZE, LR_DEFAULTSIZE, 0))};
    if (!RegisterClassEx(&window_class))
    {
        return nullptr;
    }
    _className = window_class_name;

    _hwnd = CreateWindowEx(WS_EX_ACCEPTFILES, window_class_name, window_title,
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
                           _instance, this);
    if (!_hwnd)
    {
        return nullptr;
    }
    ShowWindow(_hwnd, SW_SHOWDEFAULT);

    return _hwnd;
}

LRESULT CALLBACK Win32Window::Proc(HWND hwnd, UINT msg, WPARAM wparam,
                                   LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    case WM_SIZE:
    {
        if (wparam != SIZE_MINIMIZED)
        {
            RaiseEvent({
                .type = WindowEventTypes::SizeChanged,
                .width = LOWORD(lparam),
                .height = HIWORD(lparam),
            });
        }
        return 0;
    }

    case WM_MOVE:
    {
        RECT window_rect;
        DwmGetWindowAttribute(
            hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect,
            sizeof(RECT)); // Get window position without shadows
        HMONITOR monitor = MonitorFromPoint({window_rect.left, window_rect.top},
                                            MONITOR_DEFAULTTONEAREST);
        UINT current_dpi = 0;
        GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &current_dpi,
                         &current_dpi);
        if (current_dpi != _saved_dpi_scaling)
        {
            float dpi_scale = static_cast<float>(current_dpi) /
                              static_cast<float>(_saved_dpi_scaling);
            GetWindowRect(hwnd,
                          &window_rect); // Window RECT with shadows
            int new_window_width =
                (window_rect.right - window_rect.left) * dpi_scale + 0.5f;
            int new_window_height =
                (window_rect.bottom - window_rect.top) * dpi_scale + 0.5f;

            // Make sure window is not larger than the actual
            // monitor
            MONITORINFO monitor_info;
            monitor_info.cbSize = sizeof(monitor_info);
            GetMonitorInfo(monitor, &monitor_info);
            uint32_t monitor_width =
                monitor_info.rcWork.right - monitor_info.rcWork.left;
            uint32_t monitor_height =
                monitor_info.rcWork.bottom - monitor_info.rcWork.top;
            if (new_window_width > monitor_width)
                new_window_width = monitor_width;
            if (new_window_height > monitor_height)
                new_window_height = monitor_height;

            SetWindowPos(hwnd, nullptr, 0, 0, new_window_width,
                         new_window_height, SWP_NOMOVE | SWP_NOOWNERZORDER);

            if (current_dpi != _saved_dpi_scaling)
            {
                _saved_dpi_scaling = current_dpi;
                RaiseEvent({
                    .type = WindowEventTypes::DpiChanged,
                    .dpi = current_dpi,
                });
            }
        }
        return 0;
    }

    case WM_RENDERER_FONT_UPDATE:
    {
        // auto [rows, cols] = context->renderer->GridSize();
        // context->nvim->SendResize(rows, cols);
        return 0;
    }

    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
    {
        _dead_char_pending = true;
        return 0;
    }
    case WM_CHAR:
    {
        _dead_char_pending = false;
        RaiseEvent({
            .type = WindowEventTypes::KeyChar,
            .key = static_cast<wchar_t>(wparam),
        });
        return 0;
    }
    case WM_SYSCHAR:
    {
        _dead_char_pending = false;
        RaiseEvent({
            .type = WindowEventTypes::KeySysChar,
            .key = static_cast<wchar_t>(wparam),
        });
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        // Special case for <ALT+ENTER> (fullscreen transition)
        if (((GetKeyState(VK_MENU) & 0x80) != 0) && wparam == VK_RETURN)
        {
            // context->ToggleFullscreen();
        }
        else
        {
            LONG msg_pos = GetMessagePos();
            POINTS pt = MAKEPOINTS(msg_pos);
            MSG current_msg{.hwnd = hwnd,
                            .message = msg,
                            .wParam = wparam,
                            .lParam = lparam,
                            .time = static_cast<DWORD>(GetMessageTime()),
                            .pt = POINT{pt.x, pt.y}};

            if (_dead_char_pending)
            {
                if (static_cast<int>(wparam) == VK_SPACE ||
                    static_cast<int>(wparam) == VK_BACK ||
                    static_cast<int>(wparam) == VK_ESCAPE)
                {
                    _dead_char_pending = false;
                    TranslateMessage(&current_msg);
                    return 0;
                }
            }

            // If none of the special keys were hit, process in
            // WM_CHAR

            auto key = Nvim::GetNvimKey(static_cast<int>(wparam));
            if (key)
            {
                RaiseEvent({
                    .type = WindowEventTypes::KeyModified,
                    .modified = key,
                });
            }
            else
            {
                TranslateMessage(&current_msg);
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        POINTS cursor_pos = MAKEPOINTS(lparam);
        RaiseEvent({
            .type = WindowEventTypes::MouseMove,
            .cursor_pos = cursor_pos,
        });
        return 0;
    }

    case WM_LBUTTONDOWN:
        RaiseEvent({
            .type = WindowEventTypes::MouseLeftDown,
            .cursor_pos = MAKEPOINTS(lparam),
        });
        return 0;

    case WM_RBUTTONDOWN:
        RaiseEvent({
            .type = WindowEventTypes::MouseRightDown,
            .cursor_pos = MAKEPOINTS(lparam),
        });
        return 0;

    case WM_MBUTTONDOWN:
        RaiseEvent({
            .type = WindowEventTypes::MouseMiddleDown,
            .cursor_pos = MAKEPOINTS(lparam),
        });
        return 0;

    case WM_LBUTTONUP:
        RaiseEvent({
            .type = WindowEventTypes::MouseLeftUp,
            .cursor_pos = MAKEPOINTS(lparam),
        });
        return 0;

    case WM_RBUTTONUP:
        RaiseEvent({
            .type = WindowEventTypes::MouseRightUp,
            .cursor_pos = MAKEPOINTS(lparam),
        });
        return 0;

    case WM_MBUTTONUP:
        RaiseEvent({
            .type = WindowEventTypes::MouseMiddleUp,
            .cursor_pos = MAKEPOINTS(lparam),
        });
        return 0;

    case WM_XBUTTONDOWN:
    {
        int button = GET_XBUTTON_WPARAM(wparam);
        // if (button == XBUTTON1 && !context->xbuttons[0])
        // {
        //     context->nvim->SendInput("<C-o>");
        //     context->xbuttons[0] = true;
        // }
        // else if (button == XBUTTON2 && !context->xbuttons[1])
        // {
        //     context->nvim->SendInput("<C-i>");
        //     context->xbuttons[1] = true;
        // }
        return 0;
    }
    case WM_XBUTTONUP:
    {
        int button = GET_XBUTTON_WPARAM(wparam);
        // if (button == XBUTTON1)
        // {
        //     context->xbuttons[0] = false;
        // }
        // else if (button == XBUTTON2)
        // {
        //     context->xbuttons[1] = false;
        // }
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        bool should_resize_font = (GetKeyState(VK_CONTROL) & 0x80) != 0;

        POINTS screen_point = MAKEPOINTS(lparam);
        POINT client_point{
            .x = static_cast<LONG>(screen_point.x),
            .y = static_cast<LONG>(screen_point.y),
        };
        ScreenToClient(hwnd, &client_point);

        short wheel_distance = GET_WHEEL_DELTA_WPARAM(wparam);
        short scroll_amount = wheel_distance / WHEEL_DELTA;

        RaiseEvent({
            .type = WindowEventTypes::MouseWheel,
            .client_point = client_point,
            .scroll_amount = scroll_amount,
            .should_resize_font = should_resize_font,
        });
        return 0;
    }

    case WM_DROPFILES:
    {
        wchar_t file_to_open[MAX_PATH];
        uint32_t num_files = DragQueryFileW(reinterpret_cast<HDROP>(wparam),
                                            0xFFFFFFFF, file_to_open, MAX_PATH);
        for (int i = 0; i < num_files; ++i)
        {
            DragQueryFileW(reinterpret_cast<HDROP>(wparam), i, file_to_open,
                           MAX_PATH);
            RaiseEvent({
                .type = WindowEventTypes::FileDroped,
                .path = file_to_open,
            });
        }
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

bool Win32Window::ProcessMessage()
{
    MSG msg;
    if (!GetMessage(&msg, 0, 0, 0))
    {
        // exit loop
        return false;
    }

    // TranslateMessage(&msg);
    DispatchMessage(&msg);

    return true;
}

void Win32Window::OnEvent(
    const std::function<void(const WindowEvent &)> &callback)
{
    _callbacks.push_back(callback);
}

void Win32Window::RaiseEvent(const WindowEvent &event)
{
    for (auto &callback : _callbacks)
    {
        callback(event);
    }
}
