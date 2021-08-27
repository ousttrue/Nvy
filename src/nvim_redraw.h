#pragma once

namespace msgpackpp {
class parser;
}

struct NvimRedraw {
  bool _ui_busy = false;

  void Dispatch(class Grid *grid, class Renderer *renderer,
                const msgpackpp::parser &params);

private:
  void SetGuiOptions(class Renderer *renderer,
                     const msgpackpp::parser &option_set);
  void UpdateGridSize(Grid *grid, const msgpackpp::parser &grid_resize);
  void UpdateCursorPos(Grid *grid, const msgpackpp::parser &cursor_goto);
  void UpdateCursorModeInfos(Grid *grid,
                             const msgpackpp::parser &mode_info_set_params);
  void UpdateCursorMode(Grid *grid, const msgpackpp::parser &mode_change);
  void UpdateDefaultColors(Grid *grid, const msgpackpp::parser &default_colors);
  void UpdateHighlightAttributes(Grid *grid,
                                 const msgpackpp::parser &highlight_attribs);
  void DrawGridLines(Grid *grid, Renderer *renderer,
                     const msgpackpp::parser &grid_lines);
  void ScrollRegion(Grid *grid, Renderer *renderer,
                    const msgpackpp::parser &scroll_region);
};
