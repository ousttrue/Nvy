#pragma once
#include <d2d1_3.h>
#include <functional>

using on_rows_cols_t = std::function<void(int, int)>;
struct HighlightAttribute;
class Grid;
class Renderer {
  class RendererImpl *_impl = nullptr;

public:
  Renderer(HWND hwnd, bool disable_ligatures, float linespace_factor,
           const HighlightAttribute *defaultHL);
  ~Renderer();
  // window
  D2D1_SIZE_U Size() const;
  void Resize(uint32_t width, uint32_t height);
  D2D1_SIZE_U FontSize() const;
  void UpdateGuiFont(const char *guifont, size_t strlen);
  D2D1_SIZE_U GridToPixelSize(int rows, int cols);
  void OnRowsCols(const on_rows_cols_t &callback);
  // render
  void DrawBackgroundRect(int rows, int cols, const HighlightAttribute *hl);
  void DrawGridLine(const Grid *grid, int row);
  void DrawCursor(const Grid *grid);
  void DrawBorderRectangles(const Grid *grid);
  void StartDraw();
  void FinishDraw();
};
