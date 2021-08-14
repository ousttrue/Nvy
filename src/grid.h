#pragma once
#include <vector>
#include <functional>
#include <list>
#include "cursor.h"
#include "hl.h"

constexpr int MAX_CURSOR_MODE_INFOS = 64;

struct CellProperty
{
    uint16_t hl_attrib_id;
    bool is_wide_char;
};

struct GridPoint
{
    int row;
    int col;

    static GridPoint FromCursor(int x, int y, int font_width, int font_height)
    {
        return GridPoint{.row = static_cast<int>(y / font_height),
                         .col = static_cast<int>(x / font_width)};
    }
};

struct GridSize
{
    int rows;
    int cols;

    bool operator==(const GridSize &rhs) const
    {
        return rows == rhs.rows && cols == rhs.cols;
    }

    static GridSize FromWindowSize(int window_width, int window_height,
                                   int font_width, int font_height)
    {
        return GridSize{.rows = static_cast<int>(window_height / font_height),
                        .cols = static_cast<int>(window_width / font_width)};
    }
};
using GridSizeChanged = std::function<void(const GridSize &)>;

class Grid
{
    GridSize _size = {};
    std::vector<wchar_t> _grid_chars;
    std::vector<CellProperty> _grid_cell_properties;
    CursorModeInfo _cursor_mode_infos[MAX_CURSOR_MODE_INFOS] = {};
    Cursor _cursor = {0};
    std::list<GridSizeChanged> _sizeCallbacks;
    HighlightAttributes _hl;

public:
    Grid();
    ~Grid();
    Grid(const Grid &) = delete;
    Grid &operator=(const Grid &) = delete;

    void OnSizeChanged(const GridSizeChanged &callback)
    {
        _sizeCallbacks.push_back(callback);
    }

    int Rows() const
    {
        return _size.rows;
    }
    int Cols() const
    {
        return _size.cols;
    }
    int Count() const
    {
        return _size.cols * _size.rows;
    }
    wchar_t *Chars()
    {
        return _grid_chars.data();
    }
    const wchar_t *Chars() const
    {
        return _grid_chars.data();
    }
    CellProperty *Props()
    {
        return _grid_cell_properties.data();
    }
    const CellProperty *Props() const
    {
        return _grid_cell_properties.data();
    }
    void Resize(const GridSize &size);
    void LineCopy(int left, int right, int src_row, int dst_row);
    void Clear();

    void SetCursor(int row, int col)
    {
        _cursor.row = row;
        _cursor.col = col;
    }
    int CursorRow() const
    {
        return _cursor.row;
    }
    int CursorCol() const
    {
        return _cursor.col;
    }
    CursorShape GetCursorShape() const
    {
        if (_cursor.mode_info)
        {
            return _cursor.mode_info->shape;
        }
        else
        {
            return CursorShape::None;
        }
    }
    void SetCursorShape(int i, CursorShape shape)
    {
        this->_cursor_mode_infos[i].shape = shape;
    }
    int CursorOffset() const
    {
        return _cursor.row * _size.cols + _cursor.col;
    }
    int CursorModeHighlightAttribute() const
    {
        return this->_cursor.mode_info->hl_attrib_id;
    }
    void SetCursorModeHighlightAttribute(int i, int id)
    {
        this->_cursor_mode_infos[i].hl_attrib_id = id;
    }
    void SetCursorModeInfo(size_t index)
    {
        this->_cursor.mode_info = &this->_cursor_mode_infos[index];
    }

    HighlightAttribute &hl(size_t index)
    {
        return _hl[index];
    }
    const HighlightAttribute &hl(size_t index) const
    {
        return _hl[index];
    }
};
