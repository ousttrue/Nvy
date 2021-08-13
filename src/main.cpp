#include "commandline.h"
#include "third_party/mpack/mpack.h"
#include "win32window.h"
#include "nvim/nvim.h"
#include "renderer/renderer.h"

auto WINDOW_CLASS = L"Nvy_Class";
auto WINDOW_TITLE = L"Nvy";

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

            case WindowEventTypes::KeyChar:
                // Special case for <LT>
                if (event.key == 0x3C)
                {
                    nvim.SendInput("<LT>");
                }
                else
                {
                    nvim.SendChar(event.key);
                }
                break;

            case WindowEventTypes::KeySysChar:
                nvim.SendSysChar(event.key);
                break;

            case WindowEventTypes::KeyModified:
                nvim.ProcessKeyDown(event.modified);
                break;

            case WindowEventTypes::MouseMove:
            {
                auto grid_pos = renderer.CursorToGridPoint(event.cursor_pos.x,
                                                           event.cursor_pos.y);
                // if (context->cached_cursor_grid_pos.col != grid_pos.col ||
                //     context->cached_cursor_grid_pos.row != grid_pos.row)
                // {
                //     switch (wparam)
                //     {
                //     case MK_LBUTTON:
                //     {
                //         context->nvim->SendMouseInput(
                //             MouseButton::Left, MouseAction::Drag,
                //             grid_pos.row, grid_pos.col);
                //     }
                //     break;
                //     case MK_MBUTTON:
                //     {
                //         context->nvim->SendMouseInput(
                //             MouseButton::Middle, MouseAction::Drag,
                //             grid_pos.row, grid_pos.col);
                //     }
                //     break;
                //     case MK_RBUTTON:
                //     {
                //         context->nvim->SendMouseInput(
                //             MouseButton::Right, MouseAction::Drag,
                //             grid_pos.row, grid_pos.col);
                //     }
                //     break;
                //     }
                //     context->cached_cursor_grid_pos = grid_pos;
                // }

                break;
            }

            case WindowEventTypes::MouseLeftDown:
            {
                auto [row, col] = renderer.CursorToGridPoint(
                    event.cursor_pos.x, event.cursor_pos.y);
                nvim.SendMouseInput(MouseButton::Left, MouseAction::Press, row,
                                    col);
                break;
            }
            case WindowEventTypes::MouseLeftUp:
            {
                auto [row, col] = renderer.CursorToGridPoint(
                    event.cursor_pos.x, event.cursor_pos.y);
                nvim.SendMouseInput(MouseButton::Left, MouseAction::Release,
                                    row, col);
                break;
            }

            case WindowEventTypes::MouseRightDown:
            {
                auto [row, col] = renderer.CursorToGridPoint(
                    event.cursor_pos.x, event.cursor_pos.y);
                nvim.SendMouseInput(MouseButton::Right, MouseAction::Press, row,
                                    col);
                break;
            }
            case WindowEventTypes::MouseRightUp:
            {
                auto [row, col] = renderer.CursorToGridPoint(
                    event.cursor_pos.x, event.cursor_pos.y);
                nvim.SendMouseInput(MouseButton::Right, MouseAction::Release,
                                    row, col);
                break;
            }

            case WindowEventTypes::MouseMiddleDown:
            {
                auto [row, col] = renderer.CursorToGridPoint(
                    event.cursor_pos.x, event.cursor_pos.y);
                nvim.SendMouseInput(MouseButton::Middle, MouseAction::Press,
                                    row, col);
                break;
            }
            case WindowEventTypes::MouseMiddleUp:
            {
                auto [row, col] = renderer.CursorToGridPoint(
                    event.cursor_pos.x, event.cursor_pos.y);
                nvim.SendMouseInput(MouseButton::Middle, MouseAction::Release,
                                    row, col);
                break;
            }

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

    nvim.Launch(
        cmd.nvim_command_line,
        // call from thread
        [hwnd, &nvim, &renderer](const mpack_tree_t *tree)
        {
            if (tree == nullptr)
            {
                // on exit nvim
                PostMessage(hwnd, WM_DESTROY, 0, 0);
                return;
            }

            auto result =
                MPackExtractMessageResult(const_cast<mpack_tree_t *>(tree));

            if (result.type == MPackMessageType::Response)
            {
                auto method = nvim.GetRequestFromID(result.response.msg_id);
                switch (method)
                {
                case NvimRequest::vim_get_api_info:
                {
                    mpack_node_t top_level_map =
                        mpack_node_array_at(result.params, 1);
                    mpack_node_t version_map =
                        mpack_node_map_value_at(top_level_map, 0);
                    int64_t api_level =
                        mpack_node_map_cstr(version_map, "api_level")
                            .data->value.i;
                    assert(api_level > 6);
                }
                break;
                case NvimRequest::nvim_eval:
                {
                    Vec<char> guifont_buffer;
                    nvim.ParseConfig(result.params, &guifont_buffer);

                    if (!guifont_buffer.empty())
                    {
                        renderer.UpdateGuiFont(guifont_buffer.data(),
                                               strlen(guifont_buffer.data()));
                    }

                    // if (start_grid_size.rows != 0 && start_grid_size.cols !=
                    // 0)
                    // {
                    //     PixelSize start_size = renderer->GridToPixelSize(
                    //         start_grid_size.rows, start_grid_size.cols);
                    //     RECT client_rect;
                    //     GetClientRect(hwnd, &client_rect);
                    //     MoveWindow(hwnd, client_rect.left, client_rect.top,
                    //                start_size.width, start_size.height,
                    //                false);
                    // }

                    // Attach the renderer now that the window size is
                    // determined
                    // renderer->Attach();
                    auto [rows, cols] = renderer.GridSize();
                    nvim.SendUIAttach(rows, cols);

                    // if (start_maximized)
                    // {
                    //     ToggleFullscreen();
                    // }
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
                    renderer.Redraw(result.params);
                }
            }
        });

    while (window.ProcessMessage())
    {
        renderer.Flush();
    }

    return 0;
}
