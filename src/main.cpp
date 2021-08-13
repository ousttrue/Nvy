#include "nvim.h"
#include "grid.h"
#include "renderer.h"
#include "mpack_helper.h"
#include "vec.h"
#include "window_messages.h"
#include "third_party/mpack/mpack.h"
#include <dwmapi.h>
#include <shellscalingapi.h>

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
    UINT saved_dpi_scaling;
    uint32_t saved_window_width;
    uint32_t saved_window_height;
    bool _ui_busy = false;
    Grid _grid;

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
                auto guifont_buffer = nvim->ParseConfig(&result.params);
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
                Redraw(result.params);
            }
        }
    }

    void Redraw(mpack_node_t params)
    {
        renderer->InitializeWindowDependentResources();
        renderer->StartDraw();

        uint64_t redraw_commands_length = mpack_node_array_length(params);
        for (uint64_t i = 0; i < redraw_commands_length; ++i)
        {
            mpack_node_t redraw_command_arr = mpack_node_array_at(params, i);
            mpack_node_t redraw_command_name =
                mpack_node_array_at(redraw_command_arr, 0);

            if (MPackMatchString(redraw_command_name, "option_set"))
            {
                SetGuiOptions(redraw_command_arr);
            }
            if (MPackMatchString(redraw_command_name, "grid_resize"))
            {
                UpdateGridSize(redraw_command_arr);
            }
            if (MPackMatchString(redraw_command_name, "grid_clear"))
            {
                _grid.Clear();
                renderer->DrawBackgroundRect();
            }
            else if (MPackMatchString(redraw_command_name,
                                      "default_colors_set"))
            {
                UpdateDefaultColors(redraw_command_arr);
            }
            else if (MPackMatchString(redraw_command_name, "hl_attr_define"))
            {
                UpdateHighlightAttributes(redraw_command_arr);
            }
            else if (MPackMatchString(redraw_command_name, "grid_line"))
            {
                DrawGridLines(redraw_command_arr);
            }
            else if (MPackMatchString(redraw_command_name, "grid_cursor_goto"))
            {
                // If the old cursor position is still within the row bounds,
                // redraw the line to get rid of the cursor
                if (_grid.CursorRow() < _grid.Rows())
                {
                    renderer->DrawGridLine(&_grid, _grid.CursorRow());
                }
                UpdateCursorPos(redraw_command_arr);
            }
            else if (MPackMatchString(redraw_command_name, "mode_info_set"))
            {
                UpdateCursorModeInfos(redraw_command_arr);
            }
            else if (MPackMatchString(redraw_command_name, "mode_change"))
            {
                // Redraw cursor if its inside the bounds
                if (_grid.CursorRow() < _grid.Rows())
                {
                    renderer->DrawGridLine(&_grid, _grid.CursorRow());
                }
                UpdateCursorMode(redraw_command_arr);
            }
            else if (MPackMatchString(redraw_command_name, "busy_start"))
            {
                this->_ui_busy = true;
                // Hide cursor while UI is busy
                if (_grid.CursorRow() < _grid.Rows())
                {
                    renderer->DrawGridLine(&_grid, _grid.CursorRow());
                }
            }
            else if (MPackMatchString(redraw_command_name, "busy_stop"))
            {
                this->_ui_busy = false;
            }
            else if (MPackMatchString(redraw_command_name, "grid_scroll"))
            {
                ScrollRegion(redraw_command_arr);
            }
            else if (MPackMatchString(redraw_command_name, "flush"))
            {
                if (!this->_ui_busy)
                {
                    renderer->DrawCursor();
                }
                renderer->DrawBorderRectangles();
                renderer->FinishDraw();
            }
        }
    }

    void SetGuiOptions(mpack_node_t option_set)
    {
        uint64_t option_set_length = mpack_node_array_length(option_set);

        for (uint64_t i = 1; i < option_set_length; ++i)
        {
            mpack_node_t name =
                mpack_node_array_at(mpack_node_array_at(option_set, i), 0);
            mpack_node_t value =
                mpack_node_array_at(mpack_node_array_at(option_set, i), 1);
            if (MPackMatchString(name, "guifont"))
            {
                const char *font_str = mpack_node_str(value);
                size_t strlen = mpack_node_strlen(value);
                renderer->UpdateGuiFont(font_str, strlen);

                // Send message to window in order to update nvim row/col count
                PostMessage(hwnd, WM_RENDERER_FONT_UPDATE, 0, 0);
            }
        }
    }

    void UpdateGridSize(mpack_node_t grid_resize)
    {
        mpack_node_t grid_resize_params = mpack_node_array_at(grid_resize, 1);
        int grid_cols = MPackIntFromArray(grid_resize_params, 1);
        int grid_rows = MPackIntFromArray(grid_resize_params, 2);

        _grid.Resize({grid_rows, grid_cols});
    }

    void UpdateCursorPos(mpack_node_t cursor_goto)
    {
        mpack_node_t cursor_goto_params = mpack_node_array_at(cursor_goto, 1);
        auto row = MPackIntFromArray(cursor_goto_params, 1);
        auto col = MPackIntFromArray(cursor_goto_params, 2);
        _grid.SetCursor(row, col);
    }

    void UpdateCursorModeInfos(mpack_node_t mode_info_set_params)
    {
        mpack_node_t mode_info_params =
            mpack_node_array_at(mode_info_set_params, 1);
        mpack_node_t mode_infos = mpack_node_array_at(mode_info_params, 1);
        size_t mode_infos_length = mpack_node_array_length(mode_infos);
        assert(mode_infos_length <= MAX_CURSOR_MODE_INFOS);

        for (size_t i = 0; i < mode_infos_length; ++i)
        {
            mpack_node_t mode_info_map = mpack_node_array_at(mode_infos, i);

            _grid.SetCursorShape(i, CursorShape::None);
            mpack_node_t cursor_shape =
                mpack_node_map_cstr_optional(mode_info_map, "cursor_shape");
            if (!mpack_node_is_missing(cursor_shape))
            {
                const char *cursor_shape_str = mpack_node_str(cursor_shape);
                size_t strlen = mpack_node_strlen(cursor_shape);
                if (!strncmp(cursor_shape_str, "block", strlen))
                {
                    _grid.SetCursorShape(i, CursorShape::Block);
                }
                else if (!strncmp(cursor_shape_str, "vertical", strlen))
                {
                    _grid.SetCursorShape(i, CursorShape::Vertical);
                }
                else if (!strncmp(cursor_shape_str, "horizontal", strlen))
                {
                    _grid.SetCursorShape(i, CursorShape::Horizontal);
                }
            }

            _grid.SetCursorModeHighlightAttribute(i, 0);
            mpack_node_t hl_attrib_index =
                mpack_node_map_cstr_optional(mode_info_map, "attr_id");
            if (!mpack_node_is_missing(hl_attrib_index))
            {
                _grid.SetCursorModeHighlightAttribute(
                    i, static_cast<int>(hl_attrib_index.data->value.i));
            }
        }
    }

    void UpdateCursorMode(mpack_node_t mode_change)
    {
        mpack_node_t mode_change_params = mpack_node_array_at(mode_change, 1);
        _grid.SetCursorModeInfo(
            mpack_node_array_at(mode_change_params, 1).data->value.u);
    }

    void UpdateDefaultColors(mpack_node_t default_colors)
    {
        size_t default_colors_arr_length =
            mpack_node_array_length(default_colors);

        for (size_t i = 1; i < default_colors_arr_length; ++i)
        {
            mpack_node_t color_arr = mpack_node_array_at(default_colors, i);

            // Default colors occupy the first index of the highlight attribs
            // array
            _grid.GetHighlightAttributes()[0].foreground =
                static_cast<uint32_t>(
                    mpack_node_array_at(color_arr, 0).data->value.u);
            _grid.GetHighlightAttributes()[0].background =
                static_cast<uint32_t>(
                    mpack_node_array_at(color_arr, 1).data->value.u);
            _grid.GetHighlightAttributes()[0].special = static_cast<uint32_t>(
                mpack_node_array_at(color_arr, 2).data->value.u);
            _grid.GetHighlightAttributes()[0].flags = 0;
        }
    }

    void UpdateHighlightAttributes(mpack_node_t highlight_attribs)
    {
        uint64_t attrib_count = mpack_node_array_length(highlight_attribs);
        for (uint64_t i = 1; i < attrib_count; ++i)
        {
            int64_t attrib_index =
                mpack_node_array_at(mpack_node_array_at(highlight_attribs, i),
                                    0)
                    .data->value.i;
            assert(attrib_index <= MAX_HIGHLIGHT_ATTRIBS);

            mpack_node_t attrib_map = mpack_node_array_at(
                mpack_node_array_at(highlight_attribs, i), 1);

            const auto SetColor = [&](const char *name, uint32_t *color) {
                mpack_node_t color_node =
                    mpack_node_map_cstr_optional(attrib_map, name);
                if (!mpack_node_is_missing(color_node))
                {
                    *color = static_cast<uint32_t>(color_node.data->value.u);
                }
                else
                {
                    *color = DEFAULT_COLOR;
                }
            };
            SetColor("foreground",
                     &_grid.GetHighlightAttributes()[attrib_index].foreground);
            SetColor("background",
                     &_grid.GetHighlightAttributes()[attrib_index].background);
            SetColor("special",
                     &_grid.GetHighlightAttributes()[attrib_index].special);

            const auto SetFlag = [&](const char *flag_name,
                                     HighlightAttributeFlags flag) {
                mpack_node_t flag_node =
                    mpack_node_map_cstr_optional(attrib_map, flag_name);
                if (!mpack_node_is_missing(flag_node))
                {
                    if (flag_node.data->value.b)
                    {
                        _grid.GetHighlightAttributes()[attrib_index].flags |=
                            flag;
                    }
                    else
                    {
                        _grid.GetHighlightAttributes()[attrib_index].flags &=
                            ~flag;
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

    void DrawGridLines(mpack_node_t grid_lines)
    {
        int grid_size = _grid.Count();
        size_t line_count = mpack_node_array_length(grid_lines);
        for (size_t i = 1; i < line_count; ++i)
        {
            mpack_node_t grid_line = mpack_node_array_at(grid_lines, i);

            int row = MPackIntFromArray(grid_line, 1);
            int col_start = MPackIntFromArray(grid_line, 2);

            mpack_node_t cells_array = mpack_node_array_at(grid_line, 3);
            size_t cells_array_length = mpack_node_array_length(cells_array);

            int col_offset = col_start;
            int hl_attrib_id = 0;
            for (size_t j = 0; j < cells_array_length; ++j)
            {

                mpack_node_t cells = mpack_node_array_at(cells_array, j);
                size_t cells_length = mpack_node_array_length(cells);

                mpack_node_t text = mpack_node_array_at(cells, 0);
                const char *str = mpack_node_str(text);

                int strlen = static_cast<int>(mpack_node_strlen(text));

                if (cells_length > 1)
                {
                    hl_attrib_id = MPackIntFromArray(cells, 1);
                }

                // Right part of double-width char is the empty string, thus
                // if the next cell array contains the empty string, we can
                // process the current string as a double-width char and proceed
                if (j < (cells_array_length - 1) &&
                    mpack_node_strlen(mpack_node_array_at(
                        mpack_node_array_at(cells_array, j + 1), 0)) == 0)
                {

                    int offset = row * _grid.Cols() + col_offset;
                    _grid.Props()[offset].is_wide_char = true;
                    _grid.Props()[offset].hl_attrib_id = hl_attrib_id;
                    _grid.Props()[offset + 1].hl_attrib_id = hl_attrib_id;

                    int wstrlen = MultiByteToWideChar(CP_UTF8, 0, str, strlen,
                                                      &_grid.Chars()[offset],
                                                      grid_size - offset);
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
                    repeat = MPackIntFromArray(cells, 2);
                }

                int offset = row * _grid.Cols() + col_offset;
                int wstrlen = 0;
                for (int k = 0; k < repeat; ++k)
                {
                    int idx = offset + (k * wstrlen);
                    wstrlen = MultiByteToWideChar(CP_UTF8, 0, str, strlen,
                                                  &_grid.Chars()[idx],
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

    void ScrollRegion(mpack_node_t scroll_region)
    {
        mpack_node_t scroll_region_params =
            mpack_node_array_at(scroll_region, 1);

        int64_t top =
            mpack_node_array_at(scroll_region_params, 1).data->value.i;
        int64_t bottom =
            mpack_node_array_at(scroll_region_params, 2).data->value.i;
        int64_t left =
            mpack_node_array_at(scroll_region_params, 3).data->value.i;
        int64_t right =
            mpack_node_array_at(scroll_region_params, 4).data->value.i;
        int64_t rows =
            mpack_node_array_at(scroll_region_params, 5).data->value.i;
        int64_t cols =
            mpack_node_array_at(scroll_region_params, 6).data->value.i;

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
            // model. Thus we fall back to drawing the appropriate scrolled grid
            // lines
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

            int rows, cols;
            if (context->renderer->SetDpiScale(current_dpi, &rows, &cols))
            {
                context->nvim->SendResize(rows, cols);
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
        auto [row, col] = context->renderer->CursorToGridPoint(client_point.x,
                                                               client_point.y);
        MouseAction action = scroll_amount > 0 ? MouseAction::MouseWheelUp
                                               : MouseAction::MouseWheelDown;

        if (should_resize_font)
        {
            int rows, cols;
            if (context->renderer->ResizeFont(scroll_amount * 2.0f, &rows,
                                              &cols))
            {
                context->nvim->SendResize(rows, cols);
            }
        }
        else
        {
            for (int i = 0; i < abs(scroll_amount); ++i)
            {
                context->nvim->SendMouseInput(MouseButton::Wheel, action, row,
                                              col);
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
            context->nvim->OpenFile(file_to_open);
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

    Context context{.start_grid_size{.rows = static_cast<int>(cmd.rows),
                                     .cols = static_cast<int>(cmd.cols)},
                    .start_maximized = cmd.start_maximized,

                    .nvim = nullptr,
                    .renderer = nullptr,
                    .saved_window_placement =
                        WINDOWPLACEMENT{.length = sizeof(WINDOWPLACEMENT)}};

    HWND hwnd = CreateWindowEx(WS_EX_ACCEPTFILES, window_class_name,
                               window_title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               nullptr, nullptr, instance, &context);
    if (hwnd == NULL)
    {
        return 1;
    }
    Nvim nvim(cmd.nvim_command_line, hwnd);
    context.nvim = &nvim;
    context._grid.OnSizeChanged([&nvim](const GridSize &size) {
        nvim.SendResize(size.rows, size.cols);
    });
    context.hwnd = hwnd;
    RECT window_rect;
    DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect,
                          sizeof(RECT));
    HMONITOR monitor = MonitorFromPoint({window_rect.left, window_rect.top},
                                        MONITOR_DEFAULTTONEAREST);
    GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &(context.saved_dpi_scaling),
                     &(context.saved_dpi_scaling));

    Renderer renderer(hwnd, cmd.disable_ligatures, cmd.linespace_factor,
                      context.saved_dpi_scaling, &context._grid);
    context.renderer = &renderer;

    MSG msg;
    uint32_t previous_width = 0, previous_height = 0;
    while (GetMessage(&msg, 0, 0, 0))
    {
        // TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (previous_width != context.saved_window_width ||
            previous_height != context.saved_window_height)
        {
            previous_width = context.saved_window_width;
            previous_height = context.saved_window_height;
            auto [rows, cols] = context.renderer->PixelsToGridSize(
                context.saved_window_width, context.saved_window_height);
            context.renderer->Resize(context.saved_window_width,
                                     context.saved_window_height);
            context.nvim->SendResize(rows, cols);
        }
    }

    UnregisterClass(window_class_name, instance);
    DestroyWindow(hwnd);

    return 0;
}
