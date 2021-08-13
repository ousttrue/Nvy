#include "commandline.h"
#include "win32window.h"
#include "nvim/nvim.h"
#include "renderer/renderer.h"
#include <stdint.h>
#include <string>
#include <functional>
#include <list>

auto WINDOW_CLASS = L"Nvy_Class";
auto WINDOW_TITLE = L"Nvy";

struct Context
{
    GridSize start_grid_size;
    bool start_maximized;
    HWND hwnd;
    Nvim *nvim;
    Renderer *renderer;
    bool dead_char_pending;
    bool xbuttons[2];
    GridPoint cached_cursor_grid_pos;
    WINDOWPLACEMENT saved_window_placement;
    uint32_t saved_window_width;
    uint32_t saved_window_height;

    void ToggleFullscreen()
    {
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        MONITORINFO mi{.cbSize = sizeof(MONITORINFO)};
        if (style & WS_OVERLAPPEDWINDOW)
        {
            if (GetWindowPlacement(hwnd, &saved_window_placement) &&
                GetMonitorInfo(
                    MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
            {
                SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left,
                             mi.rcMonitor.top,
                             mi.rcMonitor.right - mi.rcMonitor.left,
                             mi.rcMonitor.bottom - mi.rcMonitor.top,
                             SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }
        }
        else
        {
            SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
            SetWindowPlacement(hwnd, &saved_window_placement);
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                             SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }

    void ProcessMPackMessage(mpack_tree_t *tree)
    {
        MPackMessageResult result = MPackExtractMessageResult(tree);

        if (result.type == MPackMessageType::Response)
        {
            auto method = nvim->GetRequestFromID(result.response.msg_id);
            switch (method)
            {
            case NvimRequest::vim_get_api_info:
            {
                mpack_node_t top_level_map =
                    mpack_node_array_at(result.params, 1);
                mpack_node_t version_map =
                    mpack_node_map_value_at(top_level_map, 0);
                int64_t api_level =
                    mpack_node_map_cstr(version_map, "api_level").data->value.i;
                assert(api_level > 6);
            }
            break;
            case NvimRequest::nvim_eval:
            {
                Vec<char> guifont_buffer;
                nvim->ParseConfig(result.params, &guifont_buffer);

                if (!guifont_buffer.empty())
                {
                    renderer->UpdateGuiFont(guifont_buffer.data(),
                                            strlen(guifont_buffer.data()));
                }

                if (start_grid_size.rows != 0 && start_grid_size.cols != 0)
                {
                    PixelSize start_size = renderer->GridToPixelSize(
                        start_grid_size.rows, start_grid_size.cols);
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    MoveWindow(hwnd, client_rect.left, client_rect.top,
                               start_size.width, start_size.height, false);
                }

                // Attach the renderer now that the window size is
                // determined
                // renderer->Attach();
                auto [rows, cols] = renderer->GridSize();
                nvim->SendUIAttach(rows, cols);

                if (start_maximized)
                {
                    ToggleFullscreen();
                }
                ShowWindow(hwnd, SW_SHOWDEFAULT);
            }
            break;
            case NvimRequest::nvim_input:
            case NvimRequest::nvim_input_mouse:
            case NvimRequest::nvim_command:
            {
            }
            break;
            }
        }
        else if (result.type == MPackMessageType::Notification)
        {
            if (MPackMatchString(result.notification.name, "redraw"))
            {
                renderer->Redraw(result.params);
            }
        }
    }
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    Context *context =
        reinterpret_cast<Context *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg)
    {
    case WM_NVIM_MESSAGE:
    {
        mpack_tree_t *tree = reinterpret_cast<mpack_tree_t *>(wparam);
        context->ProcessMPackMessage(tree);
    }
        return 0;
    case WM_RENDERER_FONT_UPDATE:
    {
        auto [rows, cols] = context->renderer->GridSize();
        context->nvim->SendResize(rows, cols);
    }
        return 0;
    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
    {
        context->dead_char_pending = true;
    }
        return 0;
    case WM_CHAR:
    {
        context->dead_char_pending = false;
        // Special case for <LT>
        if (wparam == 0x3C)
        {
            context->nvim->SendInput("<LT>");
            return 0;
        }
        context->nvim->SendChar(static_cast<wchar_t>(wparam));
    }
        return 0;
    case WM_SYSCHAR:
    {
        context->dead_char_pending = false;
        context->nvim->SendSysChar(static_cast<wchar_t>(wparam));
    }
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        // Special case for <ALT+ENTER> (fullscreen transition)
        if (((GetKeyState(VK_MENU) & 0x80) != 0) && wparam == VK_RETURN)
        {
            context->ToggleFullscreen();
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

            if (context->dead_char_pending)
            {
                if (static_cast<int>(wparam) == VK_SPACE ||
                    static_cast<int>(wparam) == VK_BACK ||
                    static_cast<int>(wparam) == VK_ESCAPE)
                {
                    context->dead_char_pending = false;
                    TranslateMessage(&current_msg);
                    return 0;
                }
            }

            // If none of the special keys were hit, process in
            // WM_CHAR
            if (!context->nvim->ProcessKeyDown(static_cast<int>(wparam)))
            {
                TranslateMessage(&current_msg);
            }
        }
    }
        return 0;
    case WM_MOUSEMOVE:
    {
        POINTS cursor_pos = MAKEPOINTS(lparam);
        GridPoint grid_pos =
            context->renderer->CursorToGridPoint(cursor_pos.x, cursor_pos.y);
        if (context->cached_cursor_grid_pos.col != grid_pos.col ||
            context->cached_cursor_grid_pos.row != grid_pos.row)
        {
            switch (wparam)
            {
            case MK_LBUTTON:
            {
                context->nvim->SendMouseInput(MouseButton::Left,
                                              MouseAction::Drag, grid_pos.row,
                                              grid_pos.col);
            }
            break;
            case MK_MBUTTON:
            {
                context->nvim->SendMouseInput(MouseButton::Middle,
                                              MouseAction::Drag, grid_pos.row,
                                              grid_pos.col);
            }
            break;
            case MK_RBUTTON:
            {
                context->nvim->SendMouseInput(MouseButton::Right,
                                              MouseAction::Drag, grid_pos.row,
                                              grid_pos.col);
            }
            break;
            }
            context->cached_cursor_grid_pos = grid_pos;
        }
    }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        POINTS cursor_pos = MAKEPOINTS(lparam);
        auto [row, col] =
            context->renderer->CursorToGridPoint(cursor_pos.x, cursor_pos.y);
        if (msg == WM_LBUTTONDOWN)
        {
            context->nvim->SendMouseInput(MouseButton::Left, MouseAction::Press,
                                          row, col);
        }
        else if (msg == WM_MBUTTONDOWN)
        {
            context->nvim->SendMouseInput(MouseButton::Middle,
                                          MouseAction::Press, row, col);
        }
        else if (msg == WM_RBUTTONDOWN)
        {
            context->nvim->SendMouseInput(MouseButton::Right,
                                          MouseAction::Press, row, col);
        }
        else if (msg == WM_LBUTTONUP)
        {
            context->nvim->SendMouseInput(MouseButton::Left,
                                          MouseAction::Release, row, col);
        }
        else if (msg == WM_MBUTTONUP)
        {
            context->nvim->SendMouseInput(MouseButton::Middle,
                                          MouseAction::Release, row, col);
        }
        else if (msg == WM_RBUTTONUP)
        {
            context->nvim->SendMouseInput(MouseButton::Right,
                                          MouseAction::Release, row, col);
        }
    }
        return 0;
    case WM_XBUTTONDOWN:
    {
        int button = GET_XBUTTON_WPARAM(wparam);
        if (button == XBUTTON1 && !context->xbuttons[0])
        {
            context->nvim->SendInput("<C-o>");
            context->xbuttons[0] = true;
        }
        else if (button == XBUTTON2 && !context->xbuttons[1])
        {
            context->nvim->SendInput("<C-i>");
            context->xbuttons[1] = true;
        }
    }
        return 0;
    case WM_XBUTTONUP:
    {
        int button = GET_XBUTTON_WPARAM(wparam);
        if (button == XBUTTON1)
        {
            context->xbuttons[0] = false;
        }
        else if (button == XBUTTON2)
        {
            context->xbuttons[1] = false;
        }
    }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR p_cmd_line, int n_cmd_show)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    auto cmd = CommandLine::Get();

    Win32Window window(instance);
    Renderer renderer(cmd.disable_ligatures, cmd.linespace_factor);
    Nvim nvim;

    // connect event
    window.OnEvent(
        [&renderer, &nvim](const WindowEvent &event)
        {
            switch (event.type)
            {
            case WindowEventTypes::SizeChanged:
                renderer.Resize(event.width, event.height);
                break;

            case WindowEventTypes::DpiChanged:
                renderer.SetDpiScale(event.dpi);
                break;

            case WindowEventTypes::MouseWheel:
            {
                auto [row, col] = renderer.CursorToGridPoint(
                    event.client_point.x, event.client_point.y);
                MouseAction action = event.scroll_amount > 0
                                         ? MouseAction::MouseWheelUp
                                         : MouseAction::MouseWheelDown;

                if (event.should_resize_font)
                {
                    int rows, cols;
                    if (renderer.ResizeFont(event.scroll_amount * 2.0f, &rows,
                                            &cols))
                    {
                        nvim.SendResize(rows, cols);
                    }
                }
                else
                {
                    for (int i = 0; i < abs(event.scroll_amount); ++i)
                    {
                        nvim.SendMouseInput(MouseButton::Wheel, action, row,
                                            col);
                    }
                }
                break;
            }

            case WindowEventTypes::FileDroped:
                nvim.OpenFile(event.path);
                break;
            }
        });

    renderer.OnEvent(
        [&nvim](const RendererEvent &event)
        {
            switch (event.type)
            {
            case RendererEventTypes::GridSizeChanged:
                nvim.SendResize(event.gridSize.rows, event.gridSize.cols);
                break;
            }
        });

    // launch
    auto hwnd = window.Create(WINDOW_CLASS, WINDOW_TITLE);
    if (!hwnd)
    {
        return 1;
    }
    renderer.Attach(hwnd);

    nvim.Launch(cmd.nvim_command_line,
                // call from thread
                [hwnd](const mpack_tree_t *tree)
                {
                    if (tree == nullptr)
                    {
                        // on exit nvim
                        PostMessage(hwnd, WM_DESTROY, 0, 0);
                    }
                });

    while (window.ProcessMessage())
    {
        renderer.Flush();
    }

    return 0;
}
