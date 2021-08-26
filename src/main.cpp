#include "nvim_pipe.h"
#include "grid.h"
#include "hl.h"
#include "renderer.h"
#include <msgpackpp/msgpackpp.h>
#include "vec.h"
#include "window_messages.h"
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/DebugOutputAppender.h>

struct Context
{
    GridSize start_grid_size = {};
    bool start_maximized = false;
    HWND hwnd = nullptr;
    NvimPipe *nvim = nullptr;
    Renderer *renderer = nullptr;
    bool dead_char_pending = false;
    bool xbuttons[2] = {false, false};
    GridPoint cached_cursor_grid_pos = {};
    WINDOWPLACEMENT saved_window_placement = {.length =
                                                  sizeof(WINDOWPLACEMENT)};
    UINT saved_dpi_scaling = 0;
    uint32_t saved_window_width = 0;
    uint32_t saved_window_height = 0;
    bool _ui_busy = false;
    Grid _grid;

    Context(int rows, int cols, bool start_maximized)
        : start_grid_size({.rows = rows, .cols = cols}),
          start_maximized(start_maximized)
    {
    }

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

    std::vector<char> ParseConfig(const msgpackpp::parser &config_node)
    {
        auto p = config_node.get_string();
        std::string path(p.begin(), p.end());
        path += "\\init.vim";

        HANDLE config_file =
            CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        std::vector<char> guifont_out;
        if (config_file == INVALID_HANDLE_VALUE)
        {
            return guifont_out;
        }

        char *buffer;
        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(config_file, &file_size))
        {
            CloseHandle(config_file);
            return guifont_out;
        }
        buffer = static_cast<char *>(malloc(file_size.QuadPart));

        DWORD bytes_read;
        if (!ReadFile(config_file, buffer, file_size.QuadPart, &bytes_read,
                      NULL))
        {
            CloseHandle(config_file);
            free(buffer);
            return guifont_out;
        }
        CloseHandle(config_file);

        char *strtok_context;
        char *line = strtok_s(buffer, "\r\n", &strtok_context);
        while (line)
        {
            char *guifont = strstr(line, "set guifont=");
            if (guifont)
            {
                // Check if we're inside a comment
                int leading_count = guifont - line;
                bool inside_comment = false;
                for (int i = 0; i < leading_count; ++i)
                {
                    if (line[i] == '"')
                    {
                        inside_comment = !inside_comment;
                    }
                }
                if (!inside_comment)
                {
                    guifont_out.clear();

                    int line_offset = (guifont - line + strlen("set guifont="));
                    int guifont_strlen = strlen(line) - line_offset;
                    int escapes = 0;
                    for (int i = 0; i < guifont_strlen; ++i)
                    {
                        if (line[line_offset + i] == '\\' &&
                            i < (guifont_strlen - 1) &&
                            line[line_offset + i + 1] == ' ')
                        {
                            guifont_out.push_back(' ');
                            ++i;
                            continue;
                        }
                        guifont_out.push_back(line[i + line_offset]);
                    }
                    guifont_out.push_back('\0');
                }
            }
            line = strtok_s(NULL, "\r\n", &strtok_context);
        }

        free(buffer);
        return guifont_out;
    }

    void SendUIAttach(int grid_rows, int grid_cols)
    {
        // Send UI attach notification
        msgpackpp::packer args;
        args.pack_array(3);
        args << 190;
        args << 45;
        args.pack_map(1);
        args << "ext_linegrid" << true;
        auto msg = msgpackpp::make_rpc_notify_packed("nvim_ui_attach",
                                                     args.get_payload());
        nvim->Send(msg);
    }

    void SendResize(int grid_rows, int grid_cols)
    {
        auto msg = msgpackpp::make_rpc_notify("nvim_ui_try_resize", grid_cols,
                                              grid_rows);
        nvim->Send(msg);
    }

    void SendChar(wchar_t input_char)
    {
        // If the space is simply a regular space,
        // simply send the modified input
        if (input_char == VK_SPACE)
        {
            NvimSendModifiedInput("Space", true);
            return;
        }

        char utf8_encoded[64]{};
        if (!WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, 0, 0, NULL, NULL))
        {
            return;
        }
        WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, utf8_encoded, 64, NULL,
                            NULL);

        auto msg = msgpackpp::make_rpc_notify("nvim_input",
                                              (const char *)utf8_encoded);
        nvim->Send(msg);
    }

    void SendSysChar(wchar_t input_char)
    {
        char utf8_encoded[64]{};
        if (!WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, 0, 0, NULL, NULL))
        {
            return;
        }
        WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, utf8_encoded, 64, NULL,
                            NULL);

        NvimSendModifiedInput(utf8_encoded, true);
    }

    void NvimSendModifiedInput(const char *input, bool virtual_key)
    {
        bool shift_down = (GetKeyState(VK_SHIFT) & 0x80) != 0;
        bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x80) != 0;
        bool alt_down = (GetKeyState(VK_MENU) & 0x80) != 0;

        constexpr int MAX_INPUT_STRING_SIZE = 64;
        char input_string[MAX_INPUT_STRING_SIZE];

        snprintf(input_string, MAX_INPUT_STRING_SIZE, "<%s%s%s%s>",
                 ctrl_down ? "C-" : "", shift_down ? "S-" : "",
                 alt_down ? "M-" : "", input);

        auto msg = msgpackpp::make_rpc_notify("nvim_input",
                                              (const char *)input_string);
        nvim->Send(msg);
    }

    void SendInput(const char *input_chars)
    {
        auto msg = msgpackpp::make_rpc_notify("nvim_input", input_chars);
        nvim->Send(msg);
    }

    const char *GetMouseBotton(MouseButton button)
    {
        switch (button)
        {
        case MouseButton::Left:
            return "left";
        case MouseButton::Right:
            return "right";
        case MouseButton::Middle:
            return "middle";
        case MouseButton::Wheel:
            return "wheel";
        default:
            assert(false);
            return nullptr;
        }
    }

    const char *GetMouseAction(MouseAction action)
    {
        switch (action)
        {
        case MouseAction::Press:
            return "press";
        case MouseAction::Drag:
            return "drag";
        case MouseAction::Release:
            return "release";
        case MouseAction::MouseWheelUp:
            return "up";
        case MouseAction::MouseWheelDown:
            return "down";
        case MouseAction::MouseWheelLeft:
            return "left";
        case MouseAction::MouseWheelRight:
            return "right";
        default:
            assert(false);
            return nullptr;
        }
    }

    void SendMouseInput(MouseButton button, MouseAction action, int mouse_row,
                        int mouse_col)
    {
        bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x80) != 0;
        bool shift_down = (GetKeyState(VK_SHIFT) & 0x80) != 0;
        bool alt_down = (GetKeyState(VK_MENU) & 0x80) != 0;
        constexpr int MAX_INPUT_STRING_SIZE = 64;
        char input_string[MAX_INPUT_STRING_SIZE];
        snprintf(input_string, MAX_INPUT_STRING_SIZE, "%s%s%s",
                 ctrl_down ? "C-" : "", shift_down ? "S-" : "",
                 alt_down ? "M-" : "");

        auto msg = msgpackpp::make_rpc_notify(
            "nvim_input_mouse", GetMouseBotton(button), GetMouseAction(action),
            (const char *)input_string, 0, mouse_row, mouse_col);
        nvim->Send(msg);
    }

    bool ProcessKeyDown(int virtual_key)
    {
        const char *key;
        switch (virtual_key)
        {
        case VK_BACK:
        {
            key = "BS";
        }
        break;
        case VK_TAB:
        {
            key = "Tab";
        }
        break;
        case VK_RETURN:
        {
            key = "CR";
        }
        break;
        case VK_ESCAPE:
        {
            key = "Esc";
        }
        break;
        case VK_PRIOR:
        {
            key = "PageUp";
        }
        break;
        case VK_NEXT:
        {
            key = "PageDown";
        }
        break;
        case VK_HOME:
        {
            key = "Home";
        }
        break;
        case VK_END:
        {
            key = "End";
        }
        break;
        case VK_LEFT:
        {
            key = "Left";
        }
        break;
        case VK_UP:
        {
            key = "Up";
        }
        break;
        case VK_RIGHT:
        {
            key = "Right";
        }
        break;
        case VK_DOWN:
        {
            key = "Down";
        }
        break;
        case VK_INSERT:
        {
            key = "Insert";
        }
        break;
        case VK_DELETE:
        {
            key = "Del";
        }
        break;
        case VK_NUMPAD0:
        {
            key = "k0";
        }
        break;
        case VK_NUMPAD1:
        {
            key = "k1";
        }
        break;
        case VK_NUMPAD2:
        {
            key = "k2";
        }
        break;
        case VK_NUMPAD3:
        {
            key = "k3";
        }
        break;
        case VK_NUMPAD4:
        {
            key = "k4";
        }
        break;
        case VK_NUMPAD5:
        {
            key = "k5";
        }
        break;
        case VK_NUMPAD6:
        {
            key = "k6";
        }
        break;
        case VK_NUMPAD7:
        {
            key = "k7";
        }
        break;
        case VK_NUMPAD8:
        {
            key = "k8";
        }
        break;
        case VK_NUMPAD9:
        {
            key = "k9";
        }
        break;
        case VK_MULTIPLY:
        {
            key = "kMultiply";
        }
        break;
        case VK_ADD:
        {
            key = "kPlus";
        }
        break;
        case VK_SEPARATOR:
        {
            key = "kComma";
        }
        break;
        case VK_SUBTRACT:
        {
            key = "kMinus";
        }
        break;
        case VK_DECIMAL:
        {
            key = "kPoint";
        }
        break;
        case VK_DIVIDE:
        {
            key = "kDivide";
        }
        break;
        case VK_F1:
        {
            key = "F1";
        }
        break;
        case VK_F2:
        {
            key = "F2";
        }
        break;
        case VK_F3:
        {
            key = "F3";
        }
        break;
        case VK_F4:
        {
            key = "F4";
        }
        break;
        case VK_F5:
        {
            key = "F5";
        }
        break;
        case VK_F6:
        {
            key = "F6";
        }
        break;
        case VK_F7:
        {
            key = "F7";
        }
        break;
        case VK_F8:
        {
            key = "F8";
        }
        break;
        case VK_F9:
        {
            key = "F9";
        }
        break;
        case VK_F10:
        {
            key = "F10";
        }
        break;
        case VK_F11:
        {
            key = "F11";
        }
        break;
        case VK_F12:
        {
            key = "F12";
        }
        break;
        case VK_F13:
        {
            key = "F13";
        }
        break;
        case VK_F14:
        {
            key = "F14";
        }
        break;
        case VK_F15:
        {
            key = "F15";
        }
        break;
        case VK_F16:
        {
            key = "F16";
        }
        break;
        case VK_F17:
        {
            key = "F17";
        }
        break;
        case VK_F18:
        {
            key = "F18";
        }
        break;
        case VK_F19:
        {
            key = "F19";
        }
        break;
        case VK_F20:
        {
            key = "F20";
        }
        break;
        case VK_F21:
        {
            key = "F21";
        }
        break;
        case VK_F22:
        {
            key = "F22";
        }
        break;
        case VK_F23:
        {
            key = "F23";
        }
        break;
        case VK_F24:
        {
            key = "F24";
        }
        break;
        default:
        {
        }
            return false;
        }

        NvimSendModifiedInput(key, true);
        return true;
    }

    void OpenFile(const wchar_t *file_name)
    {
        char utf8_encoded[MAX_PATH]{};
        WideCharToMultiByte(CP_UTF8, 0, file_name, -1, utf8_encoded, MAX_PATH,
                            NULL, NULL);

        char file_command[MAX_PATH + 2] = {};
        strcpy_s(file_command, MAX_PATH, "e ");
        strcat_s(file_command, MAX_PATH - 3, utf8_encoded);

        auto msg = msgpackpp::make_rpc_request(
            nvim->RegisterRequest(nvim_command), "nvim_command",
            (const char *)file_command);
        nvim->Send(msg);
    }

    std::vector<uint8_t> m_buffer;

    void ProcessMPackMessage(const std::vector<uint8_t> &msg)
    {
        std::copy(msg.begin(), msg.end(), std::back_inserter(m_buffer));

        auto current = msgpackpp::parser(m_buffer);
        while (true)
        {
            // parse stream message
            auto next = current.next();
            if (next.is_ok())
            {
                ProcessMPackMessage(current);
                current = next.value;
            }
            else
            {
                if (next.status == msgpackpp::parse_status::empty ||
                    next.status == msgpackpp::parse_status::lack)
                {
                    break;
                }

                // critical error. cannot continue
                throw std::runtime_error("critical");
            }
        }
        auto d = current.data() - m_buffer.data();
        if (d)
        {
            // consume used data
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + d);
        }
    }

    void ProcessMPackMessage(const msgpackpp::parser &msg)
    {
        switch (msg[0].get_number<int>())
        {
        case 0:
            // request [0, msgid, method, params]
            PLOG_DEBUG << "[request#" << msg[1].get_number<int>() << ", "
                       << msg[2].get_string() << "]";
            break;
        case 1:
            // response [1, msgid, error, result]
            PLOG_DEBUG << "[response#" << msg[1].get_number<int>() << "]";
            {
                auto method =
                    nvim->GetRequestFromID(msg[1].get_number<size_t>());
                switch (method)
                {
                case NvimRequest::vim_get_api_info:
                {
                    // TODO:
                    PLOGD << msg;
                    // mpack_node_t top_level_map =
                    //     mpack_node_array_at(result.params, 1);
                    // mpack_node_t version_map =
                    //     mpack_node_map_value_at(top_level_map, 0);
                    // int64_t api_level =
                    //     mpack_node_map_cstr(version_map, "api_level")
                    //         .data->value.i;
                    // assert(api_level > 6);
                    break;
                }
                case NvimRequest::nvim_eval:
                {
                    auto guifont_buffer = ParseConfig(msg[3]);
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
                    renderer->Attach();
                    auto size = renderer->Size();
                    auto fontSize = renderer->FontSize();
                    auto grid = GridSize::FromWindowSize(
                        size.width, size.height, fontSize.width,
                        fontSize.height);
                    SendUIAttach(grid.rows, grid.cols);

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

            break;
        case 2:
            // notify [2, method, params]
            PLOG_DEBUG << "[notify, " << msg[1].get_string() << "]";
            {
                if (msg[1].get_string() == "redraw")
                {
                    Redraw(msg[2]);
                }
            }
            break;
        default:
            assert(false);
            break;
        }
    }

    void Redraw(const msgpackpp::parser &params)
    {
        renderer->InitializeWindowDependentResources();
        renderer->StartDraw();

        auto redraw_commands_length = params.count();
        auto redraw_command_arr = params.first_array_item().value;
        for (uint64_t i = 0; i < redraw_commands_length;
             ++i, redraw_command_arr = redraw_command_arr.next())
        {
            auto redraw_command_name = redraw_command_arr[0].get_string();
            if (redraw_command_name == "option_set")
            {
                SetGuiOptions(redraw_command_arr);
            }
            if (redraw_command_name == "grid_resize")
            {
                UpdateGridSize(redraw_command_arr);
            }
            if (redraw_command_name == "grid_clear")
            {
                _grid.Clear();
                renderer->DrawBackgroundRect(_grid.Rows(), _grid.Cols(),
                                             &_grid.hl(0));
            }
            else if (redraw_command_name == "default_colors_set")
            {
                UpdateDefaultColors(redraw_command_arr);
            }
            else if (redraw_command_name == "hl_attr_define")
            {
                UpdateHighlightAttributes(redraw_command_arr);
            }
            else if (redraw_command_name == "grid_line")
            {
                DrawGridLines(redraw_command_arr);
            }
            else if (redraw_command_name == "grid_cursor_goto")
            {
                // If the old cursor position is still within the row bounds,
                // redraw the line to get rid of the cursor
                if (_grid.CursorRow() < _grid.Rows())
                {
                    renderer->DrawGridLine(&_grid, _grid.CursorRow());
                }
                UpdateCursorPos(redraw_command_arr);
            }
            else if (redraw_command_name == "mode_info_set")
            {
                UpdateCursorModeInfos(redraw_command_arr);
            }
            else if (redraw_command_name == "mode_change")
            {
                // Redraw cursor if its inside the bounds
                if (_grid.CursorRow() < _grid.Rows())
                {
                    renderer->DrawGridLine(&_grid, _grid.CursorRow());
                }
                UpdateCursorMode(redraw_command_arr);
            }
            else if (redraw_command_name == "busy_start")
            {
                this->_ui_busy = true;
                // Hide cursor while UI is busy
                if (_grid.CursorRow() < _grid.Rows())
                {
                    renderer->DrawGridLine(&_grid, _grid.CursorRow());
                }
            }
            else if (redraw_command_name == "busy_stop")
            {
                this->_ui_busy = false;
            }
            else if (redraw_command_name == "grid_scroll")
            {
                ScrollRegion(redraw_command_arr);
            }
            else if (redraw_command_name == "flush")
            {
                if (!this->_ui_busy)
                {
                    renderer->DrawCursor(&_grid);
                }
                renderer->DrawBorderRectangles(&_grid);
                renderer->FinishDraw();
            }
            else
            {
                PLOGD << "unknown:" << redraw_command_name;
            }
        }
    }

    void SetGuiOptions(const msgpackpp::parser &option_set)
    {
        uint64_t option_set_length = option_set.count();

        auto item = option_set.first_array_item().value.next().value;
        for (uint64_t i = 1; i < option_set_length; ++i, item = item.next())
        {
            auto name = item[0].get_string();
            if (name == "guifont")
            {
                auto font_str = item[1].get_string();
                // size_t strlen = mpack_node_strlen(value);
                renderer->UpdateGuiFont(font_str.data(), font_str.size());

                // Send message to window in order to update nvim row/col
                PostMessage(hwnd, WM_RENDERER_FONT_UPDATE, 0, 0);
            }
        }
    }

    // ["grid_resize",[1,190,45]]
    void UpdateGridSize(const msgpackpp::parser &grid_resize)
    {
        auto grid_resize_params = grid_resize[1];
        int grid_cols = grid_resize_params[1].get_number<int>();
        int grid_rows = grid_resize_params[2].get_number<int>();
        _grid.Resize({grid_rows, grid_cols});
    }

    // ["grid_cursor_goto",[1,0,4]]
    void UpdateCursorPos(const msgpackpp::parser &cursor_goto)
    {
        auto cursor_goto_params = cursor_goto[1];
        auto row = cursor_goto_params[1].get_number<int>();
        auto col = cursor_goto_params[2].get_number<int>();
        _grid.SetCursor(row, col);
    }

    // ["mode_info_set",[true,[{"mouse_shape":0...
    void UpdateCursorModeInfos(const msgpackpp::parser &mode_info_set_params)
    {
        auto mode_info_params = mode_info_set_params[1];
        auto mode_infos = mode_info_params[1];
        size_t mode_infos_length = mode_infos.count();
        assert(mode_infos_length <= MAX_CURSOR_MODE_INFOS);

        for (size_t i = 0; i < mode_infos_length; ++i)
        {
            auto mode_info_map = mode_infos[i];
            _grid.SetCursorShape(i, CursorShape::None);

            auto cursor_shape = mode_info_map["cursor_shape"];
            if (cursor_shape.is_string())
            {
                auto cursor_shape_str = cursor_shape.get_string();
                if (cursor_shape_str == "block")
                {
                    _grid.SetCursorShape(i, CursorShape::Block);
                }
                else if (cursor_shape_str == "vertical")
                {
                    _grid.SetCursorShape(i, CursorShape::Vertical);
                }
                else if (cursor_shape_str == "horizontal")
                {
                    _grid.SetCursorShape(i, CursorShape::Horizontal);
                }
            }

            _grid.SetCursorModeHighlightAttribute(i, 0);
            auto hl_attrib_index = mode_info_map["attr_id"];
            if (hl_attrib_index.is_number())
            {
                _grid.SetCursorModeHighlightAttribute(
                    i, hl_attrib_index.get_number<int>());
            }
        }
    }

    // ["mode_change",["normal",0]]
    void UpdateCursorMode(const msgpackpp::parser &mode_change)
    {
        auto mode_change_params = mode_change[1];
        _grid.SetCursorModeInfo(mode_change_params[1].get_number<int>());
    }

    // ["default_colors_set",[1.67772e+07,0,1.67117e+07,0,0]]
    void UpdateDefaultColors(const msgpackpp::parser &default_colors)
    {
        size_t default_colors_arr_length = default_colors.count();
        for (size_t i = 1; i < default_colors_arr_length; ++i)
        {
            auto color_arr = default_colors[i];

            // Default colors occupy the first index of the highlight attribs
            // array
            auto &defaultHL = _grid.hl(0);

            defaultHL.foreground = color_arr[0].get_number<uint32_t>();
            defaultHL.background = color_arr[1].get_number<uint32_t>();
            defaultHL.special = color_arr[2].get_number<uint32_t>();
            defaultHL.flags = 0;
        }
    }

    // ["hl_attr_define",[1,{},{},[]],[2,{"foreground":1.38823e+07,"background":1.1119e+07},{"for
    void UpdateHighlightAttributes(const msgpackpp::parser &highlight_attribs)
    {
        uint64_t attrib_count = highlight_attribs.count();
        for (uint64_t i = 1; i < attrib_count; ++i)
        {
            int64_t attrib_index = highlight_attribs[i][0].get_number<int>();
            assert(attrib_index <= MAX_HIGHLIGHT_ATTRIBS);

            auto attrib_map = highlight_attribs[i][1];

            const auto SetColor = [&](const char *name, uint32_t *color)
            {
                auto color_node = attrib_map[name];
                if (color_node.is_number())
                {
                    *color = color_node.get_number<uint32_t>();
                }
                else
                {
                    *color = DEFAULT_COLOR;
                }
            };
            SetColor("foreground", &_grid.hl(attrib_index).foreground);
            SetColor("background", &_grid.hl(attrib_index).background);
            SetColor("special", &_grid.hl(attrib_index).special);

            const auto SetFlag =
                [&](const char *flag_name, HighlightAttributeFlags flag)
            {
                auto flag_node = attrib_map[flag_name];
                if (flag_node.is_bool())
                {
                    if (flag_node.get_bool())
                    {
                        _grid.hl(attrib_index).flags |= flag;
                    }
                    else
                    {
                        _grid.hl(attrib_index).flags &= ~flag;
                    }
                }
            };
            SetFlag("reverse", HL_ATTRIB_REVERSE);
            SetFlag("italic", HL_ATTRIB_ITALIC);
            SetFlag("bold", HL_ATTRIB_BOLD);
            SetFlag("strikethrough", HL_ATTRIB_STRIKETHROUGH);
            SetFlag("underline", HL_ATTRIB_UNDERLINE);
            SetFlag("undercurl", HL_ATTRIB_UNDERCURL);
        }
    }

    // ["grid_line",[1,50,193,[[" ",1]]],[1,49,193,[["4",218],["%"],[" "],["
    // ",215,2],["2"],["9"],[":"],["0"]]]]
    void DrawGridLines(const msgpackpp::parser &grid_lines)
    {
        int grid_size = _grid.Count();
        size_t line_count = grid_lines.count();
        for (size_t i = 1; i < line_count; ++i)
        {
            auto grid_line = grid_lines[i];

            int row = grid_line[1].get_number<int>();
            int col_start = grid_line[2].get_number<int>();

            auto cells_array = grid_line[3];
            size_t cells_array_length = cells_array.count();

            int col_offset = col_start;
            int hl_attrib_id = 0;
            for (size_t j = 0; j < cells_array_length; ++j)
            {
                auto cells = cells_array[j];
                size_t cells_length = cells.count();

                auto text = cells[0];
                auto str = text.get_string();
                // int strlen = static_cast<int>(mpack_node_strlen(text));
                if (cells_length > 1)
                {
                    hl_attrib_id = cells[1].get_number<int>();
                }

                // Right part of double-width char is the empty string, thus
                // if the next cell array contains the empty string, we can
                // process the current string as a double-width char and
                // proceed
                if (j < (cells_array_length - 1) &&
                    cells_array[j + 1][0].get_string().size() == 0)
                {
                    int offset = row * _grid.Cols() + col_offset;
                    _grid.Props()[offset].is_wide_char = true;
                    _grid.Props()[offset].hl_attrib_id = hl_attrib_id;
                    _grid.Props()[offset + 1].hl_attrib_id = hl_attrib_id;

                    int wstrlen = MultiByteToWideChar(
                        CP_UTF8, 0, str.data(), str.size(),
                        &_grid.Chars()[offset], grid_size - offset);
                    assert(wstrlen == 1 || wstrlen == 2);

                    if (wstrlen == 1)
                    {
                        _grid.Chars()[offset + 1] = L'\0';
                    }

                    col_offset += 2;
                    continue;
                }

                if (strlen == 0)
                {
                    continue;
                }

                int repeat = 1;
                if (cells_length > 2)
                {
                    repeat = cells[2].get_number<int>();
                }

                int offset = row * _grid.Cols() + col_offset;
                int wstrlen = 0;
                for (int k = 0; k < repeat; ++k)
                {
                    int idx = offset + (k * wstrlen);
                    wstrlen = MultiByteToWideChar(
                        CP_UTF8, 0, str.data(), str.size(), &_grid.Chars()[idx],
                        grid_size - idx);
                }

                int wstrlen_with_repetitions = wstrlen * repeat;
                for (int k = 0; k < wstrlen_with_repetitions; ++k)
                {
                    _grid.Props()[offset + k].hl_attrib_id = hl_attrib_id;
                    _grid.Props()[offset + k].is_wide_char = false;
                }

                col_offset += wstrlen_with_repetitions;
            }

            renderer->DrawGridLine(&_grid, row);
        }
    }

    void ScrollRegion(const msgpackpp::parser &scroll_region)
    {
        PLOGD << scroll_region;
        auto scroll_region_params = scroll_region[1];
        int64_t top = scroll_region_params[1].get_number<int>();
        int64_t bottom = scroll_region_params[2].get_number<int>();
        int64_t left = scroll_region_params[3].get_number<int>();
        int64_t right = scroll_region_params[4].get_number<int>();
        int64_t rows = scroll_region_params[5].get_number<int>();
        int64_t cols = scroll_region_params[6].get_number<int>();

        // Currently nvim does not support horizontal scrolling,
        // the parameter is reserved for later use
        assert(cols == 0);

        // This part is slightly cryptic, basically we're just
        // iterating from top to bottom or vice versa depending on scroll
        // direction.
        bool scrolling_down = rows > 0;
        int64_t start_row = scrolling_down ? top : bottom - 1;
        int64_t end_row = scrolling_down ? bottom - 1 : top;
        int64_t increment = scrolling_down ? 1 : -1;

        for (int64_t i = start_row;
             scrolling_down ? i <= end_row : i >= end_row; i += increment)
        {
            // Clip anything outside the scroll region
            int64_t target_row = i - rows;
            if (target_row < top || target_row >= bottom)
            {
                continue;
            }

            _grid.LineCopy(left, right, i, target_row);

            // Sadly I have given up on making use of IDXGISwapChain1::Present1
            // scroll_rects or bitmap copies. The former seems insufficient for
            // nvim since it can require multiple scrolls per frame, the latter
            // I can't seem to make work with the FLIP_SEQUENTIAL swapchain
            // model. Thus we fall back to drawing the appropriate scrolled
            // grid lines
            renderer->DrawGridLine(&_grid, target_row);
        }

        // Redraw the line which the cursor has moved to, as it is no
        // longer guaranteed that the cursor is still there
        int cursor_row = _grid.CursorRow() - rows;
        if (cursor_row >= 0 && cursor_row < _grid.Rows())
        {
            renderer->DrawGridLine(&_grid, cursor_row);
        }
    }
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    Context *context =
        reinterpret_cast<Context *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (msg == WM_CREATE)
    {
        LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
        SetWindowLongPtr(
            hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
        return 0;
    }

    switch (msg)
    {
    case WM_SIZE:
    {
        if (wparam != SIZE_MINIMIZED)
        {
            uint32_t new_width = LOWORD(lparam);
            uint32_t new_height = HIWORD(lparam);
            context->saved_window_height = new_height;
            context->saved_window_width = new_width;
        }
    }
        return 0;
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
        if (current_dpi != context->saved_dpi_scaling)
        {
            float dpi_scale = static_cast<float>(current_dpi) /
                              static_cast<float>(context->saved_dpi_scaling);
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

            auto fontSize = context->renderer->SetDpiScale(current_dpi);
            auto size = context->renderer->Size();
            auto [rows, cols] = GridSize::FromWindowSize(
                size.width, size.height, fontSize.width, fontSize.height);
            if (context->_grid.Rows() != rows || context->_grid.Cols() != rows)
            {
                context->SendResize(rows, cols);
            }
            context->saved_dpi_scaling = current_dpi;
        }
    }
        return 0;
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    }
        return 0;
    case WM_RENDERER_FONT_UPDATE:
    {
        auto size = context->renderer->Size();
        auto fontSize = context->renderer->FontSize();
        auto [rows, cols] = GridSize::FromWindowSize(
            size.width, size.height, fontSize.width, fontSize.height);
        context->SendResize(rows, cols);
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
            context->SendInput("<LT>");
            return 0;
        }
        context->SendChar(static_cast<wchar_t>(wparam));
    }
        return 0;
    case WM_SYSCHAR:
    {
        context->dead_char_pending = false;
        context->SendSysChar(static_cast<wchar_t>(wparam));
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
            if (!context->ProcessKeyDown(static_cast<int>(wparam)))
            {
                TranslateMessage(&current_msg);
            }
        }
    }
        return 0;
    case WM_MOUSEMOVE:
    {
        POINTS cursor_pos = MAKEPOINTS(lparam);
        auto fontSize = context->renderer->FontSize();
        auto grid_pos = GridPoint::FromCursor(cursor_pos.x, cursor_pos.y,
                                              fontSize.width, fontSize.height);
        if (context->cached_cursor_grid_pos.col != grid_pos.col ||
            context->cached_cursor_grid_pos.row != grid_pos.row)
        {
            switch (wparam)
            {
            case MK_LBUTTON:
            {
                context->SendMouseInput(MouseButton::Left, MouseAction::Drag,
                                        grid_pos.row, grid_pos.col);
            }
            break;
            case MK_MBUTTON:
            {
                context->SendMouseInput(MouseButton::Middle, MouseAction::Drag,
                                        grid_pos.row, grid_pos.col);
            }
            break;
            case MK_RBUTTON:
            {
                context->SendMouseInput(MouseButton::Right, MouseAction::Drag,
                                        grid_pos.row, grid_pos.col);
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
        auto fontSize = context->renderer->FontSize();
        auto [row, col] = GridPoint::FromCursor(
            cursor_pos.x, cursor_pos.y, fontSize.width, fontSize.height);
        if (msg == WM_LBUTTONDOWN)
        {
            context->SendMouseInput(MouseButton::Left, MouseAction::Press, row,
                                    col);
        }
        else if (msg == WM_MBUTTONDOWN)
        {
            context->SendMouseInput(MouseButton::Middle, MouseAction::Press,
                                    row, col);
        }
        else if (msg == WM_RBUTTONDOWN)
        {
            context->SendMouseInput(MouseButton::Right, MouseAction::Press, row,
                                    col);
        }
        else if (msg == WM_LBUTTONUP)
        {
            context->SendMouseInput(MouseButton::Left, MouseAction::Release,
                                    row, col);
        }
        else if (msg == WM_MBUTTONUP)
        {
            context->SendMouseInput(MouseButton::Middle, MouseAction::Release,
                                    row, col);
        }
        else if (msg == WM_RBUTTONUP)
        {
            context->SendMouseInput(MouseButton::Right, MouseAction::Release,
                                    row, col);
        }
    }
        return 0;
    case WM_XBUTTONDOWN:
    {
        int button = GET_XBUTTON_WPARAM(wparam);
        if (button == XBUTTON1 && !context->xbuttons[0])
        {
            context->SendInput("<C-o>");
            context->xbuttons[0] = true;
        }
        else if (button == XBUTTON2 && !context->xbuttons[1])
        {
            context->SendInput("<C-i>");
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
        auto font_size = context->renderer->FontSize();
        auto [row, col] = GridPoint::FromCursor(
            client_point.x, client_point.y, font_size.width, font_size.height);
        MouseAction action = scroll_amount > 0 ? MouseAction::MouseWheelUp
                                               : MouseAction::MouseWheelDown;

        if (should_resize_font)
        {
            auto fontSize = context->renderer->ResizeFont(scroll_amount * 2.0f);
            auto size = context->renderer->Size();
            auto [rows, cols] = GridSize::FromWindowSize(
                size.width, size.height, fontSize.width, fontSize.height);
            if (context->_grid.Rows() != rows || context->_grid.Cols() != rows)
            {
                context->SendResize(rows, cols);
            }
        }
        else
        {
            for (int i = 0; i < abs(scroll_amount); ++i)
            {
                context->SendMouseInput(MouseButton::Wheel, action, row, col);
            }
        }
    }
        return 0;
    case WM_DROPFILES:
    {
        wchar_t file_to_open[MAX_PATH];
        uint32_t num_files = DragQueryFileW(reinterpret_cast<HDROP>(wparam),
                                            0xFFFFFFFF, file_to_open, MAX_PATH);
        for (int i = 0; i < num_files; ++i)
        {
            DragQueryFileW(reinterpret_cast<HDROP>(wparam), i, file_to_open,
                           MAX_PATH);
            context->OpenFile(file_to_open);
        }
    }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

const int MAX_NVIM_CMD_LINE_SIZE = 32767;
struct CommandLine
{
    bool start_maximized = false;
    bool disable_ligatures = false;
    float linespace_factor = 1.0f;
    int64_t rows = 0;
    int64_t cols = 0;
    wchar_t nvim_command_line[MAX_NVIM_CMD_LINE_SIZE] = {};

    void Parse()
    {
        int n_args;
        auto cmd_line_args = CommandLineToArgvW(GetCommandLineW(), &n_args);
        int cmd_line_size_left =
            MAX_NVIM_CMD_LINE_SIZE - wcslen(L"nvim --embed");
        wcscpy_s(nvim_command_line, MAX_NVIM_CMD_LINE_SIZE, L"nvim --embed");

        // Skip argv[0]
        for (int i = 1; i < n_args; ++i)
        {
            if (!wcscmp(cmd_line_args[i], L"--maximize"))
            {
                start_maximized = true;
            }
            else if (!wcscmp(cmd_line_args[i], L"--disable-ligatures"))
            {
                disable_ligatures = true;
            }
            else if (!wcsncmp(cmd_line_args[i], L"--geometry=",
                              wcslen(L"--geometry=")))
            {
                wchar_t *end_ptr;
                cols = wcstol(&cmd_line_args[i][11], &end_ptr, 10);
                rows = wcstol(end_ptr + 1, nullptr, 10);
            }
            else if (!wcsncmp(cmd_line_args[i], L"--linespace-factor=",
                              wcslen(L"--linespace-factor=")))
            {
                wchar_t *end_ptr;
                float factor = wcstof(&cmd_line_args[i][19], &end_ptr);
                if (factor > 0.0f && factor < 20.0f)
                {
                    linespace_factor = factor;
                }
            }
            // Otherwise assume the argument is a filename to open
            else
            {
                size_t arg_size = wcslen(cmd_line_args[i]);
                if (arg_size <= (cmd_line_size_left + 3))
                {
                    wcscat_s(nvim_command_line, cmd_line_size_left, L" \"");
                    cmd_line_size_left -= 2;
                    wcscat_s(nvim_command_line, cmd_line_size_left,
                             cmd_line_args[i]);
                    cmd_line_size_left -= arg_size;
                    wcscat_s(nvim_command_line, cmd_line_size_left, L"\"");
                    cmd_line_size_left -= 1;
                }
            }
        }
    }

    static CommandLine Get()
    {
        CommandLine cmd;
        cmd.Parse();
        return cmd;
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR p_cmd_line, int n_cmd_show)
{
    static plog::DebugOutputAppender<plog::TxtFormatter> debugOutputAppender;
    plog::init(plog::verbose, &debugOutputAppender);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    auto cmd = CommandLine::Get();

    const wchar_t *window_class_name = L"Nvy_Class";
    const wchar_t *window_title = L"Nvy";
    WNDCLASSEX window_class{.cbSize = sizeof(WNDCLASSEX),
                            .style = CS_HREDRAW | CS_VREDRAW,
                            .lpfnWndProc = WndProc,
                            .hInstance = instance,
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
        return 1;
    }

    Context context(cmd.rows, cmd.cols, cmd.start_maximized);

    HWND hwnd = CreateWindowEx(WS_EX_ACCEPTFILES, window_class_name,
                               window_title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               nullptr, nullptr, instance, &context);
    if (hwnd == NULL)
    {
        return 2;
    }
    NvimPipe nvim;
    if (!nvim.Launch(cmd.nvim_command_line))
    {
        return 3;
    }

    // Query api info
    {
        auto msg = msgpackpp::make_rpc_request(
            nvim.RegisterRequest(vim_get_api_info), "nvim_get_api_info");
        nvim.Send(msg);
    }

    // Set g:nvy global variable
    {
        auto msg = msgpackpp::make_rpc_notify("nvim_set_var", "nvy", 1);
        nvim.Send(msg);
    }

    // Query stdpath to find the users init.vim
    {
        auto msg = msgpackpp::make_rpc_request(
            nvim.RegisterRequest(nvim_eval), "nvim_eval", "stdpath('config')");
        nvim.Send(msg);
    }

    context.nvim = &nvim;
    context._grid.OnSizeChanged([&context](const GridSize &size)
                                { context.SendResize(size.rows, size.cols); });
    context.hwnd = hwnd;
    RECT window_rect;
    DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect,
                          sizeof(RECT));
    HMONITOR monitor = MonitorFromPoint({window_rect.left, window_rect.top},
                                        MONITOR_DEFAULTTONEAREST);
    GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &(context.saved_dpi_scaling),
                     &(context.saved_dpi_scaling));

    Renderer renderer(hwnd, cmd.disable_ligatures, cmd.linespace_factor,
                      context.saved_dpi_scaling, &context._grid.hl(0));
    context.renderer = &renderer;

    MSG msg;
    uint32_t previous_width = 0, previous_height = 0;

    auto lastTime = timeGetTime();
    while (true)
    {
        // windows msg
        while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {

            if (!GetMessage(&msg, NULL, 0, 0))
            {
                return msg.wParam;
            }

            // TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (previous_width != context.saved_window_width ||
                previous_height != context.saved_window_height)
            {
                previous_width = context.saved_window_width;
                previous_height = context.saved_window_height;
                auto font_size = context.renderer->FontSize();
                auto [rows, cols] = GridSize::FromWindowSize(
                    context.saved_window_width, context.saved_window_height,
                    font_size.width, font_size.height);
                context.renderer->Resize(context.saved_window_width,
                                         context.saved_window_height);
                context.SendResize(rows, cols);
            }
        }

        // nvim
        NvimMessage nvimMessage;
        while (nvim.TryDequeue(&nvimMessage))
        {
            if (nvimMessage.empty())
            {
                // nvim exited
                PostMessage(hwnd, WM_DESTROY, 0, 0);
                break;
            }
            context.ProcessMPackMessage(nvimMessage);
        }

        auto now = timeGetTime();
        auto delta = now - lastTime;
        lastTime = now;
        if (delta < 30)
        {
            Sleep(30 - delta);
        }
    }

    UnregisterClass(window_class_name, instance);
    DestroyWindow(hwnd);

    return 0;
}
