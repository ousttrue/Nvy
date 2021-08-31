#pragma once
#include <functional>
#include <stdint.h>
#include <string_view>

using on_rows_cols_t = std::function<void(int, int)>;

struct PixelSize {
  uint32_t width;
  uint32_t height;
};
struct HighlightAttribute;
class NvimGrid;
class Renderer {
  class RendererImpl *_impl = nullptr;

public:
  Renderer(void *hwnd, bool disable_ligatures, float linespace_factor,
           const HighlightAttribute *defaultHL);
  ~Renderer();
  // window
  void Resize(uint32_t width, uint32_t height);
  void SetFont(std::string_view font, float size);
  PixelSize FontSize() const;
  void OnRowsCols(const on_rows_cols_t &callback);
  // render
  void DrawBackgroundRect(int rows, int cols, const HighlightAttribute *hl);
  void DrawGridLine(const NvimGrid *grid, int row);
  void DrawCursor(const NvimGrid *grid);
  void DrawBorderRectangles(const NvimGrid *grid);
  void StartDraw();
  void FinishDraw();
};
