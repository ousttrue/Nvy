#pragma once
#include <vector>

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

class Grid
{
    int _grid_rows = 0;
    int _grid_cols = 0;
    std::vector<wchar_t> _grid_chars;
    std::vector<CellProperty> _grid_cell_properties;
    CursorModeInfo _cursor_mode_infos[MAX_CURSOR_MODE_INFOS] = {};
    Cursor _cursor = {0};
    std::vector<HighlightAttributes> _hl_attribs;

public:
    Grid();

    int Rows() const
    {
        return _grid_rows;
    }
    int Cols() const
    {
        return _grid_cols;
    }
    int Count() const
    {
        return _grid_cols * _grid_rows;
    }
    wchar_t *Chars()
    {
        return _grid_chars.data();
    }
    CellProperty *Props()
    {
        return _grid_cell_properties.data();
    }
    void Resize(int rows, int cols);
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
        return _cursor.row * _grid_cols + _cursor.col;
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
    uint32_t CreateForegroundColor(HighlightAttributes *hl_attribs);
    uint32_t CreateBackgroundColor(HighlightAttributes *hl_attribs);
    uint32_t CreateSpecialColor(HighlightAttributes *hl_attribs);
};
