#pragma once
#include <functional>
#include <stdint.h>

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
  PixelSize Size() const;
  void Resize(uint32_t width, uint32_t height);
  PixelSize FontSize() const;
  void UpdateGuiFont(const char *guifont, size_t strlen);
  void OnRowsCols(const on_rows_cols_t &callback);
  // render
  void DrawBackgroundRect(int rows, int cols, const HighlightAttribute *hl);
  void DrawGridLine(const NvimGrid *grid, int row);
  void DrawCursor(const NvimGrid *grid);
  void DrawBorderRectangles(const NvimGrid *grid);
  void StartDraw();
  void FinishDraw();
};
