#pragma once
#include <vector>
#include <functional>
#include <list>

constexpr int MAX_CURSOR_MODE_INFOS = 64;
constexpr uint32_t DEFAULT_COLOR = 0x46464646;
constexpr int MAX_HIGHLIGHT_ATTRIBS = 0xFFFF;

struct CellProperty
{
    uint16_t hl_attrib_id;
    bool is_wide_char;
};

enum class CursorShape
{
    None,
    Block,
    Vertical,
    Horizontal
};

struct CursorModeInfo
{
    CursorShape shape;
    uint16_t hl_attrib_id;
};
struct Cursor
{
    CursorModeInfo *mode_info;
    int row;
    int col;
};

enum HighlightAttributeFlags : uint16_t
{
    HL_ATTRIB_REVERSE = 1 << 0,
    HL_ATTRIB_ITALIC = 1 << 1,
    HL_ATTRIB_BOLD = 1 << 2,
    HL_ATTRIB_STRIKETHROUGH = 1 << 3,
    HL_ATTRIB_UNDERLINE = 1 << 4,
    HL_ATTRIB_UNDERCURL = 1 << 5
};

struct HighlightAttributes
{
    uint32_t foreground;
    uint32_t background;
    uint32_t special;
    uint16_t flags;
};

struct GridSize
{
    int rows;
    int cols;

    bool operator==(const GridSize &rhs) const
    {
        return rows == rhs.rows && cols == rhs.cols;
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
    std::vector<HighlightAttributes> _hl_attribs;
    std::list<GridSizeChanged> _sizeCallbacks;

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
    int CursorModeHighlightAttribute()
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

    HighlightAttributes *GetHighlightAttributes()
    {
        return this->_hl_attribs.data();
    }
    const HighlightAttributes *GetHighlightAttributes() const
    {
        return this->_hl_attribs.data();
    }
    uint32_t CreateForegroundColor(const HighlightAttributes *hl_attribs);
    uint32_t CreateBackgroundColor(const HighlightAttributes *hl_attribs);
    uint32_t CreateSpecialColor(const HighlightAttributes *hl_attribs);
};
