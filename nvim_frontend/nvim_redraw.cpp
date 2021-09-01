#include "nvim_redraw.h"
#include "nvim_grid.h"
#include "nvim_renderer.h"
#include <Windows.h>
#include <msgpackpp/msgpackpp.h>
#include <plog/Log.h>

// font_name:h14
std::tuple<std::string_view, float>
NvimRedraw::ParseGUIFont(std::string_view guifont) {
  auto size_str = guifont.find(":h");
  if (size_str == std::string::npos) {
    return {};
  }
  std::string font_size_str(guifont.begin() + size_str + 2, guifont.end());
  auto font_size = static_cast<float>(atof(font_size_str.c_str()));
  return {guifont.substr(0, size_str), font_size};
}

void NvimRedraw::Dispatch(NvimGrid *grid, NvimRenderer *renderer,
                          const msgpackpp::parser &params) {
  auto [w, h] = renderer->StartDraw();

  auto redraw_commands_length = params.count();
  auto redraw_command_arr = params.first_array_item().value;
  for (uint64_t i = 0; i < redraw_commands_length;
       ++i, redraw_command_arr = redraw_command_arr.next()) {
    auto redraw_command_name = redraw_command_arr[0].get_string();
    if (redraw_command_name == "option_set") {
      SetGuiOptions(renderer, redraw_command_arr);
    }
    if (redraw_command_name == "grid_resize") {
      UpdateGridSize(grid, redraw_command_arr);
    }
    if (redraw_command_name == "grid_clear") {
      grid->Clear();
      renderer->DrawBackgroundRect(grid->Rows(), grid->Cols(), &grid->hl(0));
    } else if (redraw_command_name == "default_colors_set") {
      UpdateDefaultColors(grid, redraw_command_arr);
    } else if (redraw_command_name == "hl_attr_define") {
      UpdateHighlightAttributes(grid, redraw_command_arr);
    } else if (redraw_command_name == "grid_line") {
      DrawGridLines(grid, renderer, redraw_command_arr);
    } else if (redraw_command_name == "grid_cursor_goto") {
      // If the old cursor position is still within the row
      // bounds, redraw the line to get rid of the cursor
      if (grid->CursorRow() < grid->Rows()) {
        renderer->DrawGridLine(grid, grid->CursorRow());
      }
      UpdateCursorPos(grid, redraw_command_arr);
    } else if (redraw_command_name == "mode_info_set") {
      UpdateCursorModeInfos(grid, redraw_command_arr);
    } else if (redraw_command_name == "mode_change") {
      // Redraw cursor if its inside the bounds
      if (grid->CursorRow() < grid->Rows()) {
        renderer->DrawGridLine(grid, grid->CursorRow());
      }
      UpdateCursorMode(grid, redraw_command_arr);
    } else if (redraw_command_name == "busy_start") {
      this->_ui_busy = true;
      // Hide cursor while UI is busy
      if (grid->CursorRow() < grid->Rows()) {
        renderer->DrawGridLine(grid, grid->CursorRow());
      }
    } else if (redraw_command_name == "busy_stop") {
      this->_ui_busy = false;
    } else if (redraw_command_name == "grid_scroll") {
      ScrollRegion(grid, renderer, redraw_command_arr);
    } else if (redraw_command_name == "flush") {
      if (!this->_ui_busy) {
        renderer->DrawCursor(grid);
      }
      renderer->DrawBorderRectangles(grid, w, h);
      renderer->FinishDraw();
    } else {
      // PLOGD << "unknown:" << redraw_command_name;
    }
  }
}

void NvimRedraw::SetGuiOptions(NvimRenderer *renderer,
                               const msgpackpp::parser &option_set) {
  uint64_t option_set_length = option_set.count();

  auto item = option_set.first_array_item().value.next().value;
  for (uint64_t i = 1; i < option_set_length; ++i, item = item.next()) {
    auto name = item[0].get_string();
    if (name == "guifont") {
      auto [font, size] = ParseGUIFont(item[1].get_string());
      renderer->SetFont(font, size);
    }
  }
}

// ["grid_resize",[1,190,45]]
void NvimRedraw::UpdateGridSize(NvimGrid *grid,
                                const msgpackpp::parser &grid_resize) {
  auto grid_resize_params = grid_resize[1];
  int grid_cols = grid_resize_params[1].get_number<int>();
  int grid_rows = grid_resize_params[2].get_number<int>();
  grid->RowsCols(grid_rows, grid_cols);
  _sizing = false;
}

// ["grid_cursor_goto",[1,0,4]]
void NvimRedraw::UpdateCursorPos(NvimGrid *grid,
                                 const msgpackpp::parser &cursor_goto) {
  auto cursor_goto_params = cursor_goto[1];
  auto row = cursor_goto_params[1].get_number<int>();
  auto col = cursor_goto_params[2].get_number<int>();
  grid->SetCursor(row, col);
}

// ["mode_info_set",[true,[{"mouse_shape":0...
void NvimRedraw::UpdateCursorModeInfos(
    NvimGrid *grid, const msgpackpp::parser &mode_info_set_params) {
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
      grid->SetCursorModeHighlightAttribute(i,
                                            hl_attrib_index.get_number<int>());
    }
  }
}

// ["mode_change",["normal",0]]
void NvimRedraw::UpdateCursorMode(NvimGrid *grid,
                                  const msgpackpp::parser &mode_change) {
  auto mode_change_params = mode_change[1];
  grid->SetCursorModeInfo(mode_change_params[1].get_number<int>());
}

// ["default_colors_set",[1.67772e+07,0,1.67117e+07,0,0]]
void NvimRedraw::UpdateDefaultColors(NvimGrid *grid,
                                     const msgpackpp::parser &default_colors) {
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
void NvimRedraw::UpdateHighlightAttributes(
    NvimGrid *grid, const msgpackpp::parser &highlight_attribs) {
  uint64_t attrib_count = highlight_attribs.count();
  for (uint64_t i = 1; i < attrib_count; ++i) {
    int64_t attrib_index = highlight_attribs[i][0].get_number<int>();

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
void NvimRedraw::DrawGridLines(NvimGrid *grid, NvimRenderer *renderer,
                               const msgpackpp::parser &grid_lines) {
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

void NvimRedraw::ScrollRegion(NvimGrid *grid, NvimRenderer *renderer,
                              const msgpackpp::parser &scroll_region) {
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
