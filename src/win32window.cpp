#include "win32window.h"
#include <Windows.h>

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                LPARAM lparam)
{
    if (msg == WM_CREATE)
    {
        auto createStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
        SetWindowLongPtr(
            hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
        return 0;
    }

    auto p = GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (p)
    {
        auto w = reinterpret_cast<Win32Window *>(p);
        return w->Proc(hwnd, msg, wparam, lparam);
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

Win32Window::~Win32Window()
{
    DestroyWindow((HWND)_hwnd);
    UnregisterClass(_class_name.c_str(), (HINSTANCE)_instance);
}

void *Win32Window::Create(void *instance, const wchar_t *class_name,
                          const wchar_t *window_title)
{
    _instance = instance;
    _class_name = class_name;

    WNDCLASSEX window_class{.cbSize = sizeof(WNDCLASSEX),
                            .style = CS_HREDRAW | CS_VREDRAW,
                            .lpfnWndProc = ::WndProc,
                            .hInstance = (HINSTANCE)instance,
                            .hIcon = static_cast<HICON>(LoadImage(
                                GetModuleHandle(NULL), L"NVIM_ICON", IMAGE_ICON,
                                LR_DEFAULTSIZE, LR_DEFAULTSIZE, 0)),
                            .hCursor = LoadCursor(NULL, IDC_ARROW),
                            .hbrBackground = nullptr,
                            .lpszClassName = _class_name.c_str(),
                            .hIconSm = static_cast<HICON>(LoadImage(
                                GetModuleHandle(NULL), L"NVIM_ICON", IMAGE_ICON,
                                LR_DEFAULTSIZE, LR_DEFAULTSIZE, 0))};
    if (!RegisterClassEx(&window_class))
    {
        return nullptr;
    }

    _hwnd = CreateWindowEx(WS_EX_ACCEPTFILES, _class_name.c_str(), window_title,
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
                           (HINSTANCE)instance, this);
    return _hwnd;
}

uint64_t Win32Window::Proc(void *hwnd, uint32_t msg, uint64_t wparam,
                           uint64_t lparam)
{
    switch (msg)
    {
        // case WM_SIZE:
        // {
        //     if (wparam != SIZE_MINIMIZED)
        //     {
        //         uint32_t new_width = LOWORD(lparam);
        //         uint32_t new_height = HIWORD(lparam);
        //         context->saved_window_height = new_height;
        //         context->saved_window_width = new_width;
        //     }
        // }
        //     return 0;
        // case WM_MOVE:
        // {
        //     RECT window_rect;
        //     DwmGetWindowAttribute(
        //         hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect,
        //         sizeof(RECT)); // Get window position without shadows
        //     HMONITOR monitor = MonitorFromPoint(
        //         {window_rect.left, window_rect.top},
        //         MONITOR_DEFAULTTONEAREST);
        //     UINT current_dpi = 0;
        //     GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &current_dpi,
        //                      &current_dpi);
        //     if (current_dpi != context->saved_dpi_scaling)
        //     {
        //         float dpi_scale =
        //             static_cast<float>(current_dpi) /
        //             static_cast<float>(context->saved_dpi_scaling);
        //         GetWindowRect(hwnd,
        //                       &window_rect); // Window RECT with shadows
        //         int new_window_width =
        //             (window_rect.right - window_rect.left) * dpi_scale +
        //             0.5f;
        //         int new_window_height =
        //             (window_rect.bottom - window_rect.top) * dpi_scale +
        //             0.5f;

        //         // Make sure window is not larger than the actual
        //         // monitor
        //         MONITORINFO monitor_info;
        //         monitor_info.cbSize = sizeof(monitor_info);
        //         GetMonitorInfo(monitor, &monitor_info);
        //         uint32_t monitor_width =
        //             monitor_info.rcWork.right - monitor_info.rcWork.left;
        //         uint32_t monitor_height =
        //             monitor_info.rcWork.bottom - monitor_info.rcWork.top;
        //         if (new_window_width > monitor_width)
        //             new_window_width = monitor_width;
        //         if (new_window_height > monitor_height)
        //             new_window_height = monitor_height;

        //         SetWindowPos(hwnd, nullptr, 0, 0, new_window_width,
        //                      new_window_height, SWP_NOMOVE |
        //                      SWP_NOOWNERZORDER);

        //         auto fontSize =
        //         context->renderer->SetDpiScale(current_dpi); auto size =
        //         context->renderer->Size(); auto [rows, cols] =
        //         GridSize::FromWindowSize(
        //             size.width, size.height, fontSize.width,
        //             fontSize.height);
        //         if (context->_grid.Rows() != rows ||
        //             context->_grid.Cols() != rows)
        //         {
        //             context->SendResize(rows, cols);
        //         }
        //         context->saved_dpi_scaling = current_dpi;
        //     }
        // }
        //     return 0;
        // case WM_DESTROY:
        // {
        //     PostQuitMessage(0);
        // }
        //     return 0;
        // case WM_RENDERER_FONT_UPDATE:
        // {
        //     auto size = context->renderer->Size();
        //     auto fontSize = context->renderer->FontSize();
        //     auto [rows, cols] = GridSize::FromWindowSize(
        //         size.width, size.height, fontSize.width,
        //         fontSize.height);
        //     context->SendResize(rows, cols);
        // }
        //     return 0;
        // case WM_DEADCHAR:
        // case WM_SYSDEADCHAR:
        // {
        //     context->dead_char_pending = true;
        // }
        //     return 0;
        // case WM_CHAR:
        // {
        //     context->dead_char_pending = false;
        //     // Special case for <LT>
        //     if (wparam == 0x3C)
        //     {
        //         context->SendInput("<LT>");
        //         return 0;
        //     }
        //     context->SendChar(static_cast<wchar_t>(wparam));
        // }
        //     return 0;
        // case WM_SYSCHAR:
        // {
        //     context->dead_char_pending = false;
        //     context->SendSysChar(static_cast<wchar_t>(wparam));
        // }
        //     return 0;
        // case WM_KEYDOWN:
        // case WM_SYSKEYDOWN:
        // {
        //     // Special case for <ALT+ENTER> (fullscreen transition)
        //     if (((GetKeyState(VK_MENU) & 0x80) != 0) && wparam ==
        //     VK_RETURN)
        //     {
        //         context->ToggleFullscreen();
        //     }
        //     else
        //     {
        //         LONG msg_pos = GetMessagePos();
        //         POINTS pt = MAKEPOINTS(msg_pos);
        //         MSG current_msg{.hwnd = hwnd,
        //                         .message = msg,
        //                         .wParam = wparam,
        //                         .lParam = lparam,
        //                         .time =
        //                         static_cast<DWORD>(GetMessageTime()), .pt
        //                         = POINT{pt.x, pt.y}};

        //         if (context->dead_char_pending)
        //         {
        //             if (static_cast<int>(wparam) == VK_SPACE ||
        //                 static_cast<int>(wparam) == VK_BACK ||
        //                 static_cast<int>(wparam) == VK_ESCAPE)
        //             {
        //                 context->dead_char_pending = false;
        //                 TranslateMessage(&current_msg);
        //                 return 0;
        //             }
        //         }

        //         // If none of the special keys were hit, process in
        //         // WM_CHAR
        //         if (!context->ProcessKeyDown(static_cast<int>(wparam)))
        //         {
        //             TranslateMessage(&current_msg);
        //         }
        //     }
        // }
        //     return 0;
        // case WM_MOUSEMOVE:
        // {
        //     POINTS cursor_pos = MAKEPOINTS(lparam);
        //     auto fontSize = context->renderer->FontSize();
        //     auto grid_pos = GridPoint::FromCursor(
        //         cursor_pos.x, cursor_pos.y, fontSize.width,
        //         fontSize.height);
        //     if (context->cached_cursor_grid_pos.col != grid_pos.col ||
        //         context->cached_cursor_grid_pos.row != grid_pos.row)
        //     {
        //         switch (wparam)
        //         {
        //         case MK_LBUTTON:
        //         {
        //             context->SendMouseInput(MouseButton::Left,
        //                                     MouseAction::Drag,
        //                                     grid_pos.row, grid_pos.col);
        //         }
        //         break;
        //         case MK_MBUTTON:
        //         {
        //             context->SendMouseInput(MouseButton::Middle,
        //                                     MouseAction::Drag,
        //                                     grid_pos.row, grid_pos.col);
        //         }
        //         break;
        //         case MK_RBUTTON:
        //         {
        //             context->SendMouseInput(MouseButton::Right,
        //                                     MouseAction::Drag,
        //                                     grid_pos.row, grid_pos.col);
        //         }
        //         break;
        //         }
        //         context->cached_cursor_grid_pos = grid_pos;
        //     }
        // }
        //     return 0;
        // case WM_LBUTTONDOWN:
        // case WM_RBUTTONDOWN:
        // case WM_MBUTTONDOWN:
        // case WM_LBUTTONUP:
        // case WM_RBUTTONUP:
        // case WM_MBUTTONUP:
        // {
        //     POINTS cursor_pos = MAKEPOINTS(lparam);
        //     auto fontSize = context->renderer->FontSize();
        //     auto [row, col] = GridPoint::FromCursor(
        //         cursor_pos.x, cursor_pos.y, fontSize.width,
        //         fontSize.height);
        //     if (msg == WM_LBUTTONDOWN)
        //     {
        //         context->SendMouseInput(MouseButton::Left,
        //         MouseAction::Press,
        //                                 row, col);
        //     }
        //     else if (msg == WM_MBUTTONDOWN)
        //     {
        //         context->SendMouseInput(MouseButton::Middle,
        //         MouseAction::Press,
        //                                 row, col);
        //     }
        //     else if (msg == WM_RBUTTONDOWN)
        //     {
        //         context->SendMouseInput(MouseButton::Right,
        //         MouseAction::Press,
        //                                 row, col);
        //     }
        //     else if (msg == WM_LBUTTONUP)
        //     {
        //         context->SendMouseInput(MouseButton::Left,
        //         MouseAction::Release,
        //                                 row, col);
        //     }
        //     else if (msg == WM_MBUTTONUP)
        //     {
        //         context->SendMouseInput(MouseButton::Middle,
        //                                 MouseAction::Release, row, col);
        //     }
        //     else if (msg == WM_RBUTTONUP)
        //     {
        //         context->SendMouseInput(MouseButton::Right,
        //                                 MouseAction::Release, row, col);
        //     }
        // }
        //     return 0;
        // case WM_XBUTTONDOWN:
        // {
        //     int button = GET_XBUTTON_WPARAM(wparam);
        //     if (button == XBUTTON1 && !context->xbuttons[0])
        //     {
        //         context->SendInput("<C-o>");
        //         context->xbuttons[0] = true;
        //     }
        //     else if (button == XBUTTON2 && !context->xbuttons[1])
        //     {
        //         context->SendInput("<C-i>");
        //         context->xbuttons[1] = true;
        //     }
        // }
        //     return 0;
        // case WM_XBUTTONUP:
        // {
        //     int button = GET_XBUTTON_WPARAM(wparam);
        //     if (button == XBUTTON1)
        //     {
        //         context->xbuttons[0] = false;
        //     }
        //     else if (button == XBUTTON2)
        //     {
        //         context->xbuttons[1] = false;
        //     }
        // }
        //     return 0;
        // case WM_MOUSEWHEEL:
        // {
        //     bool should_resize_font = (GetKeyState(VK_CONTROL) & 0x80) !=
        //     0;

        //     POINTS screen_point = MAKEPOINTS(lparam);
        //     POINT client_point{
        //         .x = static_cast<LONG>(screen_point.x),
        //         .y = static_cast<LONG>(screen_point.y),
        //     };
        //     ScreenToClient(hwnd, &client_point);

        //     short wheel_distance = GET_WHEEL_DELTA_WPARAM(wparam);
        //     short scroll_amount = wheel_distance / WHEEL_DELTA;
        //     auto font_size = context->renderer->FontSize();
        //     auto [row, col] =
        //         GridPoint::FromCursor(client_point.x, client_point.y,
        //                               font_size.width, font_size.height);
        //     MouseAction action = scroll_amount > 0
        //                              ? MouseAction::MouseWheelUp
        //                              : MouseAction::MouseWheelDown;

        //     if (should_resize_font)
        //     {
        //         auto fontSize =
        //             context->renderer->ResizeFont(scroll_amount * 2.0f);
        //         auto size = context->renderer->Size();
        //         auto [rows, cols] = GridSize::FromWindowSize(
        //             size.width, size.height, fontSize.width,
        //             fontSize.height);
        //         if (context->_grid.Rows() != rows ||
        //             context->_grid.Cols() != rows)
        //         {
        //             context->SendResize(rows, cols);
        //         }
        //     }
        //     else
        //     {
        //         for (int i = 0; i < abs(scroll_amount); ++i)
        //         {
        //             context->SendMouseInput(MouseButton::Wheel, action,
        //             row,
        //                                     col);
        //         }
        //     }
        // }
        //     return 0;
        // case WM_DROPFILES:
        // {
        //     wchar_t file_to_open[MAX_PATH];
        //     uint32_t num_files =
        //         DragQueryFileW(reinterpret_cast<HDROP>(wparam),
        //         0xFFFFFFFF,
        //                        file_to_open, MAX_PATH);
        //     for (int i = 0; i < num_files; ++i)
        //     {
        //         DragQueryFileW(reinterpret_cast<HDROP>(wparam), i,
        //         file_to_open,
        //                        MAX_PATH);
        //         context->OpenFile(file_to_open);
        //     }
        // }
        //     return 0;
    }

    return DefWindowProc((HWND)hwnd, msg, wparam, lparam);
}
