#pragma once
#include <string_view>

struct HighlightAttribute;
class NvimGrid;
class Renderer {
  class RendererImpl *_impl = nullptr;

public:
  Renderer(bool disable_ligatures, float linespace_factor, uint32_t monitor_dpi,
           const HighlightAttribute *defaultHL);
  ~Renderer();
  // font size
  void SetFont(std::string_view font, float size);
  std::tuple<float, float> FontSize() const;
  // render
  void DrawBackgroundRect(int rows, int cols, const HighlightAttribute *hl);
  void DrawGridLine(const NvimGrid *grid, int row);
  void DrawCursor(const NvimGrid *grid);
  void DrawBorderRectangles(const NvimGrid *grid, int width, int height);
  std::tuple<int, int> StartDraw(struct ID3D11Device2 *device,
                                 struct IDXGISurface2 *backbuffer);
  void FinishDraw();
};
