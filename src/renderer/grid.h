#pragma once
#include <vector>

struct CellProperty
{
    uint16_t hl_attrib_id;
    bool is_wide_char;
};

class GridImpl
{
    int _grid_rows = 0;
    int _grid_cols = 0;
    std::vector<wchar_t> _grid_chars;
    std::vector<CellProperty> _grid_cell_properties;

public:
    int Rows() const
    {
        return this->_grid_rows;
    }
    int Cols() const
    {
        return this->_grid_cols;
    }
    int Count() const
    {
        return this->_grid_cols * this->_grid_rows;
    }
    wchar_t *Chars()
    {
        return this->_grid_chars.data();
    }
    CellProperty *Props()
    {
        return this->_grid_cell_properties.data();
    }
    void Resize(int rows, int cols);
    void LineCopy(int left, int right, int src_row, int dst_row);
    void Clear();
};
