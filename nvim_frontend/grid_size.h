#pragma once
#include <functional>

struct GridSize {
  int rows;
  int cols;

  bool operator==(const GridSize &rhs) const {
    return rows == rhs.rows && cols == rhs.cols;
  }

  static GridSize FromWindowSize(int window_width, int window_height,
                                 int font_width, int font_height) {
    return GridSize{.rows = static_cast<int>(window_height / font_height),
                    .cols = static_cast<int>(window_width / font_width)};
  }
};
using GridSizeChanged = std::function<void(const GridSize &)>;
constexpr int MAX_CURSOR_MODE_INFOS = 64;

struct GridPoint {
  int row;
  int col;

  static GridPoint FromCursor(int x, int y, int font_width, int font_height) {
    return GridPoint{.row = static_cast<int>(y / font_height),
                     .col = static_cast<int>(x / font_width)};
  }
};
