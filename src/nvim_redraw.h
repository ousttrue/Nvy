#pragma once
#include <string_view>
#include <tuple>

namespace msgpackpp {
class parser;
}

struct NvimRedraw {
  bool _ui_busy = false;

  void Dispatch(struct ID3D11Device2 *device, struct IDXGISurface2 *target,
                class NvimGrid *grid, class Renderer *renderer,
                const msgpackpp::parser &params);
  static std::tuple<std::string_view, float>
  ParseGUIFont(std::string_view gui_font);

  bool _sizing = false;
  bool Sizing() const { return _sizing; }
  void SetSizing() { _sizing = true; }

private:
  void SetGuiOptions(class Renderer *renderer,
                     const msgpackpp::parser &option_set);
  void UpdateGridSize(NvimGrid *grid, const msgpackpp::parser &grid_resize);
  void UpdateCursorPos(NvimGrid *grid, const msgpackpp::parser &cursor_goto);
  void UpdateCursorModeInfos(NvimGrid *grid,
                             const msgpackpp::parser &mode_info_set_params);
  void UpdateCursorMode(NvimGrid *grid, const msgpackpp::parser &mode_change);
  void UpdateDefaultColors(NvimGrid *grid,
                           const msgpackpp::parser &default_colors);
  void UpdateHighlightAttributes(NvimGrid *grid,
                                 const msgpackpp::parser &highlight_attribs);
  void DrawGridLines(NvimGrid *grid, Renderer *renderer,
                     const msgpackpp::parser &grid_lines);
  void ScrollRegion(NvimGrid *grid, Renderer *renderer,
                    const msgpackpp::parser &scroll_region);
};
