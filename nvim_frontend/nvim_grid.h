#pragma once
#include "grid_size.h"
#include <functional>
#include <list>
#include <stdint.h>
#include <vector>

enum HighlightAttributeFlags : uint16_t {
  HL_ATTRIB_REVERSE = 1 << 0,
  HL_ATTRIB_ITALIC = 1 << 1,
  HL_ATTRIB_BOLD = 1 << 2,
  HL_ATTRIB_STRIKETHROUGH = 1 << 3,
  HL_ATTRIB_UNDERLINE = 1 << 4,
  HL_ATTRIB_UNDERCURL = 1 << 5
};

constexpr uint32_t DEFAULT_COLOR = 0x46464646;

struct HighlightAttribute {
  uint32_t foreground;
  uint32_t background;
  uint32_t special;
  uint16_t flags;
  HighlightAttribute *_default;

  uint32_t CreateForegroundColor() const {
    if (this->flags & HL_ATTRIB_REVERSE) {
      return this->background == DEFAULT_COLOR ? this->_default->background
                                               : this->background;
    } else {
      return this->foreground == DEFAULT_COLOR ? this->_default->foreground
                                               : this->foreground;
    }
  }

  uint32_t CreateBackgroundColor() const {
    if (this->flags & HL_ATTRIB_REVERSE) {
      return this->foreground == DEFAULT_COLOR ? this->_default->foreground
                                               : this->foreground;
    } else {
      return this->background == DEFAULT_COLOR ? this->_default->background
                                               : this->background;
    }
  }

  uint32_t CreateSpecialColor() const {
    return this->special == DEFAULT_COLOR ? this->_default->special
                                          : this->special;
  }
};

using HighlightAttributes = std::vector<HighlightAttribute>;

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

struct CellProperty {
  uint16_t hl_attrib_id;
  bool is_wide_char;
};

class NvimGrid {
  GridSize _size = {};
  std::vector<wchar_t> _grid_chars;
  std::vector<CellProperty> _grid_cell_properties;
  CursorModeInfo _cursor_mode_infos[MAX_CURSOR_MODE_INFOS] = {};
  Cursor _cursor = {0};
  std::list<GridSizeChanged> _sizeCallbacks;
  HighlightAttributes _hl;

public:
  NvimGrid();
  ~NvimGrid();
  NvimGrid(const NvimGrid &) = delete;
  NvimGrid &operator=(const NvimGrid &) = delete;
  int Rows() const { return _size.rows; }
  int Cols() const { return _size.cols; }
  GridSize Size() const { return _size; }
  int Count() const { return _size.cols * _size.rows; }
  wchar_t *Chars() { return _grid_chars.data(); }
  const wchar_t *Chars() const { return _grid_chars.data(); }
  CellProperty *Props() { return _grid_cell_properties.data(); }
  const CellProperty *Props() const { return _grid_cell_properties.data(); }
  bool RowsCols(int rows, int cols);
  void LineCopy(int left, int right, int src_row, int dst_row);
  void Clear();

  void SetCursor(int row, int col) {
    _cursor.row = row;
    _cursor.col = col;
  }
  int CursorRow() const { return _cursor.row; }
  int CursorCol() const { return _cursor.col; }
  CursorShape GetCursorShape() const {
    if (_cursor.mode_info) {
      return _cursor.mode_info->shape;
    } else {
      return CursorShape::None;
    }
  }
  void SetCursorShape(int i, CursorShape shape) {
    this->_cursor_mode_infos[i].shape = shape;
  }
  int CursorOffset() const { return _cursor.row * _size.cols + _cursor.col; }
  int CursorModeHighlightAttribute() const {
    return this->_cursor.mode_info->hl_attrib_id;
  }
  void SetCursorModeHighlightAttribute(int i, int id) {
    this->_cursor_mode_infos[i].hl_attrib_id = id;
  }
  void SetCursorModeInfo(size_t index) {
    this->_cursor.mode_info = &this->_cursor_mode_infos[index];
  }

  HighlightAttribute &hl(size_t index) { return _hl[index]; }
  const HighlightAttribute &hl(size_t index) const { return _hl[index]; }
};
