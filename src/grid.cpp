#include "grid.h"

Grid::Grid() {
  _hl.resize(MAX_HIGHLIGHT_ATTRIBS);
  for (auto &hl : _hl) {
    hl._default = &_hl[0];
  }
}

Grid::~Grid() {}

void Grid::RowsCols(int rows, int cols) {
  GridSize size{rows, cols};
  if (size != _size) {
    _size = size;

    auto count = Count();
    _grid_chars.resize(count);
    // Initialize all grid character to a space. An empty
    // grid cell is equivalent to a space in a text layout
    std::fill(_grid_chars.begin(), _grid_chars.end(), L' ');

    _grid_cell_properties.resize(count);

    for (auto &callback : _sizeCallbacks) {
      callback(size);
    }
  }
}

void Grid::LineCopy(int left, int right, int src_row, int dst_row) {
  memcpy(&this->_grid_chars[dst_row * this->_size.cols + left],
         &this->_grid_chars[src_row * this->_size.cols + left],
         (right - left) * sizeof(wchar_t));

  memcpy(&this->_grid_cell_properties[dst_row * this->_size.cols + left],
         &this->_grid_cell_properties[src_row * this->_size.cols + left],
         (right - left) * sizeof(CellProperty));
}

void Grid::Clear() {
  // Initialize all grid character to a space.
  for (int i = 0; i < this->_size.cols * this->_size.rows; ++i) {
    this->_grid_chars[i] = L' ';
  }
  memset(this->_grid_cell_properties.data(), 0,
         this->_size.cols * this->_size.rows * sizeof(CellProperty));
}
