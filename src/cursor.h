#pragma once
#include <stdint.h>

enum class CursorShape { None, Block, Vertical, Horizontal };

struct CursorModeInfo {
  CursorShape shape;
  uint16_t hl_attrib_id;
};

struct Cursor {
  CursorModeInfo *mode_info;
  int row;
  int col;
};
