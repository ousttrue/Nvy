#pragma once
#include <string_view>

struct HighlightAttribute;
class NvimGrid;
class NvimRenderer {
public:
  virtual ~NvimRenderer(){};
  // font size
  virtual void SetFont(std::string_view font, float size) = 0;
  virtual std::tuple<float, float> FontSize() const = 0;
  // render
  virtual void DrawBackgroundRect(int rows, int cols,
                                  const HighlightAttribute *hl) = 0;
  virtual void DrawGridLine(const NvimGrid *grid, int row) = 0;
  virtual void DrawCursor(const NvimGrid *grid) = 0;
  virtual void DrawBorderRectangles(const NvimGrid *grid, int width,
                                    int height) = 0;
  virtual std::tuple<int, int> StartDraw() = 0;
  virtual void FinishDraw() = 0;
};
