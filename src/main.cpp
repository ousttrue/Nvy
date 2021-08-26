#include "grid.h"
#include "nvim_frontend.h"
#include "renderer.h"
#include "vec.h"
#include "win32window.h"
#include "window_messages.h"
#include <Windows.h>
#include <msgpackpp/msgpackpp.h>
#include <msgpackpp/rpc.h>
#include <msgpackpp/windows_pipe_transport.h>
#include <plog/Appenders/DebugOutputAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>
#include <shellapi.h>

const int MAX_NVIM_CMD_LINE_SIZE = 32767;
struct CommandLine {
  bool start_maximized = false;
  bool disable_ligatures = false;
  float linespace_factor = 1.0f;
  int64_t rows = 0;
  int64_t cols = 0;
  wchar_t nvim_command_line[MAX_NVIM_CMD_LINE_SIZE] = {};

  void Parse() {
    int n_args;
    auto cmd_line_args = CommandLineToArgvW(GetCommandLineW(), &n_args);
    int cmd_line_size_left = MAX_NVIM_CMD_LINE_SIZE - wcslen(L"nvim --embed");
    wcscpy_s(nvim_command_line, MAX_NVIM_CMD_LINE_SIZE, L"nvim --embed");

    // Skip argv[0]
    for (int i = 1; i < n_args; ++i) {
      if (!wcscmp(cmd_line_args[i], L"--maximize")) {
        start_maximized = true;
      } else if (!wcscmp(cmd_line_args[i], L"--disable-ligatures")) {
        disable_ligatures = true;
      } else if (!wcsncmp(cmd_line_args[i], L"--geometry=",
                          wcslen(L"--geometry="))) {
        wchar_t *end_ptr;
        cols = wcstol(&cmd_line_args[i][11], &end_ptr, 10);
        rows = wcstol(end_ptr + 1, nullptr, 10);
      } else if (!wcsncmp(cmd_line_args[i], L"--linespace-factor=",
                          wcslen(L"--linespace-factor="))) {
        wchar_t *end_ptr;
        float factor = wcstof(&cmd_line_args[i][19], &end_ptr);
        if (factor > 0.0f && factor < 20.0f) {
          linespace_factor = factor;
        }
      }
      // Otherwise assume the argument is a filename to open
      else {
        size_t arg_size = wcslen(cmd_line_args[i]);
        if (arg_size <= (cmd_line_size_left + 3)) {
          wcscat_s(nvim_command_line, cmd_line_size_left, L" \"");
          cmd_line_size_left -= 2;
          wcscat_s(nvim_command_line, cmd_line_size_left, cmd_line_args[i]);
          cmd_line_size_left -= arg_size;
          wcscat_s(nvim_command_line, cmd_line_size_left, L"\"");
          cmd_line_size_left -= 1;
        }
      }
    }
  }

  static CommandLine Get() {
    CommandLine cmd;
    cmd.Parse();
    return cmd;
  }
};

struct RedrawDispatch {
  Renderer *renderer = nullptr;
  Grid *grid = nullptr;
  HWND hwnd = nullptr;
  bool _ui_busy = false;

  void Dispatch(const msgpackpp::parser &params) {
    renderer->InitializeWindowDependentResources();
    renderer->StartDraw();

    auto redraw_commands_length = params.count();
    auto redraw_command_arr = params.first_array_item().value;
    for (uint64_t i = 0; i < redraw_commands_length;
         ++i, redraw_command_arr = redraw_command_arr.next()) {
      auto redraw_command_name = redraw_command_arr[0].get_string();
      if (redraw_command_name == "option_set") {
        SetGuiOptions(redraw_command_arr);
      }
      if (redraw_command_name == "grid_resize") {
        UpdateGridSize(redraw_command_arr);
      }
      if (redraw_command_name == "grid_clear") {
        grid->Clear();
        renderer->DrawBackgroundRect(grid->Rows(), grid->Cols(), &grid->hl(0));
      } else if (redraw_command_name == "default_colors_set") {
        UpdateDefaultColors(redraw_command_arr);
      } else if (redraw_command_name == "hl_attr_define") {
        UpdateHighlightAttributes(redraw_command_arr);
      } else if (redraw_command_name == "grid_line") {
        DrawGridLines(redraw_command_arr);
      } else if (redraw_command_name == "grid_cursor_goto") {
        // If the old cursor position is still within the row
        // bounds, redraw the line to get rid of the cursor
        if (grid->CursorRow() < grid->Rows()) {
          renderer->DrawGridLine(grid, grid->CursorRow());
        }
        UpdateCursorPos(redraw_command_arr);
      } else if (redraw_command_name == "mode_info_set") {
        UpdateCursorModeInfos(redraw_command_arr);
      } else if (redraw_command_name == "mode_change") {
        // Redraw cursor if its inside the bounds
        if (grid->CursorRow() < grid->Rows()) {
          renderer->DrawGridLine(grid, grid->CursorRow());
        }
        UpdateCursorMode(redraw_command_arr);
      } else if (redraw_command_name == "busy_start") {
        this->_ui_busy = true;
        // Hide cursor while UI is busy
        if (grid->CursorRow() < grid->Rows()) {
          renderer->DrawGridLine(grid, grid->CursorRow());
        }
      } else if (redraw_command_name == "busy_stop") {
        this->_ui_busy = false;
      } else if (redraw_command_name == "grid_scroll") {
        ScrollRegion(redraw_command_arr);
      } else if (redraw_command_name == "flush") {
        if (!this->_ui_busy) {
          renderer->DrawCursor(grid);
        }
        renderer->DrawBorderRectangles(grid);
        renderer->FinishDraw();
      } else {
        PLOGD << "unknown:" << redraw_command_name;
      }
    }
  }

private:
  void SetGuiOptions(const msgpackpp::parser &option_set) {
    uint64_t option_set_length = option_set.count();

    auto item = option_set.first_array_item().value.next().value;
    for (uint64_t i = 1; i < option_set_length; ++i, item = item.next()) {
      auto name = item[0].get_string();
      if (name == "guifont") {
        auto font_str = item[1].get_string();
        // size_t strlen = mpack_node_strlen(value);
        renderer->UpdateGuiFont(font_str.data(), font_str.size());

        // Send message to window in order to update nvim row/col
        PostMessage(hwnd, WM_RENDERER_FONT_UPDATE, 0, 0);
      }
    }
  }

  // ["grid_resize",[1,190,45]]
  void UpdateGridSize(const msgpackpp::parser &grid_resize) {
    auto grid_resize_params = grid_resize[1];
    int grid_cols = grid_resize_params[1].get_number<int>();
    int grid_rows = grid_resize_params[2].get_number<int>();
    grid->Resize({grid_rows, grid_cols});
  }

  // ["grid_cursor_goto",[1,0,4]]
  void UpdateCursorPos(const msgpackpp::parser &cursor_goto) {
    auto cursor_goto_params = cursor_goto[1];
    auto row = cursor_goto_params[1].get_number<int>();
    auto col = cursor_goto_params[2].get_number<int>();
    grid->SetCursor(row, col);
  }

  // ["mode_info_set",[true,[{"mouse_shape":0...
  void UpdateCursorModeInfos(const msgpackpp::parser &mode_info_set_params) {
    auto mode_info_params = mode_info_set_params[1];
    auto mode_infos = mode_info_params[1];
    size_t mode_infos_length = mode_infos.count();
    assert(mode_infos_length <= MAX_CURSOR_MODE_INFOS);

    for (size_t i = 0; i < mode_infos_length; ++i) {
      auto mode_info_map = mode_infos[i];
      grid->SetCursorShape(i, CursorShape::None);

      auto cursor_shape = mode_info_map["cursor_shape"];
      if (cursor_shape.is_string()) {
        auto cursor_shape_str = cursor_shape.get_string();
        if (cursor_shape_str == "block") {
          grid->SetCursorShape(i, CursorShape::Block);
        } else if (cursor_shape_str == "vertical") {
          grid->SetCursorShape(i, CursorShape::Vertical);
        } else if (cursor_shape_str == "horizontal") {
          grid->SetCursorShape(i, CursorShape::Horizontal);
        }
      }

      grid->SetCursorModeHighlightAttribute(i, 0);
      auto hl_attrib_index = mode_info_map["attr_id"];
      if (hl_attrib_index.is_number()) {
        grid->SetCursorModeHighlightAttribute(
            i, hl_attrib_index.get_number<int>());
      }
    }
  }

  // ["mode_change",["normal",0]]
  void UpdateCursorMode(const msgpackpp::parser &mode_change) {
    auto mode_change_params = mode_change[1];
    grid->SetCursorModeInfo(mode_change_params[1].get_number<int>());
  }

  // ["default_colors_set",[1.67772e+07,0,1.67117e+07,0,0]]
  void UpdateDefaultColors(const msgpackpp::parser &default_colors) {
    size_t default_colors_arr_length = default_colors.count();
    for (size_t i = 1; i < default_colors_arr_length; ++i) {
      auto color_arr = default_colors[i];

      // Default colors occupy the first index of the highlight attribs
      // array
      auto &defaultHL = grid->hl(0);

      defaultHL.foreground = color_arr[0].get_number<uint32_t>();
      defaultHL.background = color_arr[1].get_number<uint32_t>();
      defaultHL.special = color_arr[2].get_number<uint32_t>();
      defaultHL.flags = 0;
    }
  }

  // ["hl_attr_define",[1,{},{},[]],[2,{"foreground":1.38823e+07,"background":1.1119e+07},{"for
  void UpdateHighlightAttributes(const msgpackpp::parser &highlight_attribs) {
    uint64_t attrib_count = highlight_attribs.count();
    for (uint64_t i = 1; i < attrib_count; ++i) {
      int64_t attrib_index = highlight_attribs[i][0].get_number<int>();
      assert(attrib_index <= MAX_HIGHLIGHT_ATTRIBS);

      auto attrib_map = highlight_attribs[i][1];

      const auto SetColor = [&](const char *name, uint32_t *color) {
        auto color_node = attrib_map[name];
        if (color_node.is_number()) {
          *color = color_node.get_number<uint32_t>();
        } else {
          *color = DEFAULT_COLOR;
        }
      };
      SetColor("foreground", &grid->hl(attrib_index).foreground);
      SetColor("background", &grid->hl(attrib_index).background);
      SetColor("special", &grid->hl(attrib_index).special);

      const auto SetFlag = [&](const char *flag_name,
                               HighlightAttributeFlags flag) {
        auto flag_node = attrib_map[flag_name];
        if (flag_node.is_bool()) {
          if (flag_node.get_bool()) {
            grid->hl(attrib_index).flags |= flag;
          } else {
            grid->hl(attrib_index).flags &= ~flag;
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
  void DrawGridLines(const msgpackpp::parser &grid_lines) {
    int grid_size = grid->Count();
    size_t line_count = grid_lines.count();
    for (size_t i = 1; i < line_count; ++i) {
      auto grid_line = grid_lines[i];

      int row = grid_line[1].get_number<int>();
      int col_start = grid_line[2].get_number<int>();

      auto cells_array = grid_line[3];
      size_t cells_array_length = cells_array.count();

      int col_offset = col_start;
      int hl_attrib_id = 0;
      for (size_t j = 0; j < cells_array_length; ++j) {
        auto cells = cells_array[j];
        size_t cells_length = cells.count();

        auto text = cells[0];
        auto str = text.get_string();
        // int strlen = static_cast<int>(mpack_node_strlen(text));
        if (cells_length > 1) {
          hl_attrib_id = cells[1].get_number<int>();
        }

        // Right part of double-width char is the empty string, thus
        // if the next cell array contains the empty string, we can
        // process the current string as a double-width char and
        // proceed
        if (j < (cells_array_length - 1) &&
            cells_array[j + 1][0].get_string().size() == 0) {
          int offset = row * grid->Cols() + col_offset;
          grid->Props()[offset].is_wide_char = true;
          grid->Props()[offset].hl_attrib_id = hl_attrib_id;
          grid->Props()[offset + 1].hl_attrib_id = hl_attrib_id;

          int wstrlen =
              MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(),
                                  &grid->Chars()[offset], grid_size - offset);
          assert(wstrlen == 1 || wstrlen == 2);

          if (wstrlen == 1) {
            grid->Chars()[offset + 1] = L'\0';
          }

          col_offset += 2;
          continue;
        }

        if (strlen == 0) {
          continue;
        }

        int repeat = 1;
        if (cells_length > 2) {
          repeat = cells[2].get_number<int>();
        }

        int offset = row * grid->Cols() + col_offset;
        int wstrlen = 0;
        for (int k = 0; k < repeat; ++k) {
          int idx = offset + (k * wstrlen);
          wstrlen = MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(),
                                        &grid->Chars()[idx], grid_size - idx);
        }

        int wstrlen_with_repetitions = wstrlen * repeat;
        for (int k = 0; k < wstrlen_with_repetitions; ++k) {
          grid->Props()[offset + k].hl_attrib_id = hl_attrib_id;
          grid->Props()[offset + k].is_wide_char = false;
        }

        col_offset += wstrlen_with_repetitions;
      }

      renderer->DrawGridLine(grid, row);
    }
  }

  void ScrollRegion(const msgpackpp::parser &scroll_region) {
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

    for (int64_t i = start_row; scrolling_down ? i <= end_row : i >= end_row;
         i += increment) {
      // Clip anything outside the scroll region
      int64_t target_row = i - rows;
      if (target_row < top || target_row >= bottom) {
        continue;
      }

      grid->LineCopy(left, right, i, target_row);

      // Sadly I have given up on making use of IDXGISwapChain1::Present1
      // scroll_rects or bitmap copies. The former seems insufficient for
      // nvim since it can require multiple scrolls per frame, the latter
      // I can't seem to make work with the FLIP_SEQUENTIAL swapchain
      // model. Thus we fall back to drawing the appropriate scrolled
      // grid lines
      renderer->DrawGridLine(grid, target_row);
    }

    // Redraw the line which the cursor has moved to, as it is no
    // longer guaranteed that the cursor is still there
    int cursor_row = grid->CursorRow() - rows;
    if (cursor_row >= 0 && cursor_row < grid->Rows()) {
      renderer->DrawGridLine(grid, cursor_row);
    }
  }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR p_cmd_line, int n_cmd_show) {
  static plog::DebugOutputAppender<plog::TxtFormatter> debugOutputAppender;
  plog::init(plog::verbose, &debugOutputAppender);

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  auto cmd = CommandLine::Get();

  Win32Window window;
  auto hwnd = (HWND)window.Create(instance, L"Nvy_Class", L"Nvy");
  if (!hwnd) {
    return 1;
  }

  Grid grid;
  // context._grid.OnSizeChanged([&context](const GridSize &size) {
  //   context.SendResize(size.rows, size.cols);
  // });

  Renderer renderer(hwnd, cmd.disable_ligatures, cmd.linespace_factor,
                    &grid.hl(0));

  NvimFrontend nvim;
  if (!nvim.Launch(cmd.nvim_command_line)) {
    return 3;
  }

  // setfont
  auto guifont_buffer = nvim.Initialize();
  if (!guifont_buffer.empty()) {
    renderer.UpdateGuiFont(guifont_buffer.data(), guifont_buffer.size());
  }

  // initial window size
  if (cmd.start_maximized) {
    window.ToggleFullscreen();
  } else if (cmd.rows != 0 && cmd.cols != 0) {
    PixelSize start_size = renderer.GridToPixelSize(cmd.rows, cmd.cols);
    window.Resize(start_size.width, start_size.height);
  }
  ShowWindow(hwnd, SW_SHOWDEFAULT);

  // Attach the renderer now that the window size is
  // determined
  renderer.Attach();
  auto size = renderer.Size();
  auto fontSize = renderer.FontSize();
  auto gridSize = GridSize::FromWindowSize(size.width, size.height, fontSize.width,
                                       fontSize.height);

  // //
  // window._on_resize = [&context](int w, int h) {
  //   context.saved_window_height = h;
  //   context.saved_window_width = w;
  // };
  // window._on_input = [&context](auto input) {
  //   context.SendInput(input.data());
  // };
  // window._on_char = [&context](auto ch) { context.SendChar(ch); };
  // window._on_sys_char = [&context](auto ch) { context.SendSysChar(ch); };
  // window._on_toggle_screen = [&context]() { context.ToggleFullscreen(); };
  // window._on_modified_input = [&context](auto input) {
  //   context.NvimSendModifiedInput(input.data(), true);
  // };
  // // mouse drag
  // window._on_mouse_left_drag = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto grid_pos =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   if (context.cached_cursor_grid_pos.col != grid_pos.col ||
  //       context.cached_cursor_grid_pos.row != grid_pos.row) {
  //     context.SendMouseInput(MouseButton::Left, MouseAction::Drag, grid_pos.row,
  //                            grid_pos.col);
  //     context.cached_cursor_grid_pos = grid_pos;
  //   }
  // };
  // window._on_mouse_middle_drag = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto grid_pos =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   if (context.cached_cursor_grid_pos.col != grid_pos.col ||
  //       context.cached_cursor_grid_pos.row != grid_pos.row) {
  //     context.SendMouseInput(MouseButton::Middle, MouseAction::Drag,
  //                            grid_pos.row, grid_pos.col);
  //     context.cached_cursor_grid_pos = grid_pos;
  //   }
  // };
  // window._on_mouse_right_drag = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto grid_pos =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   if (context.cached_cursor_grid_pos.col != grid_pos.col ||
  //       context.cached_cursor_grid_pos.row != grid_pos.row) {
  //     context.SendMouseInput(MouseButton::Right, MouseAction::Drag,
  //                            grid_pos.row, grid_pos.col);
  //     context.cached_cursor_grid_pos = grid_pos;
  //   }
  // };
  // // mouse button
  // window._on_mouse_left_down = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Left, MouseAction::Press, row, col);
  // };
  // window._on_mouse_middle_down = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Middle, MouseAction::Press, row, col);
  // };
  // window._on_mouse_right_down = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Right, MouseAction::Press, row, col);
  // };
  // window._on_mouse_left_release = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Left, MouseAction::Release, row, col);
  // };
  // window._on_mouse_middle_release = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Middle, MouseAction::Release, row, col);
  // };
  // window._on_mouse_right_release = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Right, MouseAction::Release, row, col);
  // };

  while (window.Loop()) {
    nvim.Process();
  }

  return 0;
}
