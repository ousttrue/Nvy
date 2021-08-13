#include "grid.h"

GridImpl::GridImpl()
{
    this->_hl_attribs.resize(MAX_HIGHLIGHT_ATTRIBS);
}

void GridImpl::Resize(int rows, int cols)
{
    if (rows != _grid_rows || cols != _grid_cols)
    {
        _grid_cols = cols;
        _grid_rows = rows;

        auto count = Count();
        _grid_chars.resize(count);
        // Initialize all grid character to a space. An empty
        // grid cell is equivalent to a space in a text layout
        std::fill(_grid_chars.begin(), _grid_chars.end(), L' ');

        _grid_cell_properties.resize(count);
    }
}

void GridImpl::LineCopy(int left, int right, int src_row, int dst_row)
{
    memcpy(&this->_grid_chars[dst_row * this->_grid_cols + left],
           &this->_grid_chars[src_row * this->_grid_cols + left],
           (right - left) * sizeof(wchar_t));

    memcpy(&this->_grid_cell_properties[dst_row * this->_grid_cols + left],
           &this->_grid_cell_properties[src_row * this->_grid_cols + left],
           (right - left) * sizeof(CellProperty));
}

void GridImpl::Clear()
{
    // Initialize all grid character to a space.
    for (int i = 0; i < this->_grid_cols * this->_grid_rows; ++i)
    {
        this->_grid_chars[i] = L' ';
    }
    memset(this->_grid_cell_properties.data(), 0,
           this->_grid_cols * this->_grid_rows * sizeof(CellProperty));
}

uint32_t GridImpl::CreateForegroundColor(HighlightAttributes *hl_attribs)
{
    if (hl_attribs->flags & HL_ATTRIB_REVERSE)
    {
        return hl_attribs->background == DEFAULT_COLOR
                   ? this->_hl_attribs[0].background
                   : hl_attribs->background;
    }
    else
    {
        return hl_attribs->foreground == DEFAULT_COLOR
                   ? this->_hl_attribs[0].foreground
                   : hl_attribs->foreground;
    }
}

uint32_t GridImpl::CreateBackgroundColor(HighlightAttributes *hl_attribs)
{
    if (hl_attribs->flags & HL_ATTRIB_REVERSE)
    {
        return hl_attribs->foreground == DEFAULT_COLOR
                   ? this->_hl_attribs[0].foreground
                   : hl_attribs->foreground;
    }
    else
    {
        return hl_attribs->background == DEFAULT_COLOR
                   ? this->_hl_attribs[0].background
                   : hl_attribs->background;
    }
}

uint32_t GridImpl::CreateSpecialColor(HighlightAttributes *hl_attribs)
{
    return hl_attribs->special == DEFAULT_COLOR ? this->_hl_attribs[0].special
                                                : hl_attribs->special;
}
