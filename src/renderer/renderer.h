#pragma once

constexpr const char *DEFAULT_FONT = "Consolas";
constexpr float DEFAULT_FONT_SIZE = 14.0f;

constexpr uint32_t DEFAULT_COLOR = 0x46464646;
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

enum class CursorShape
{
    None,
    Block,
    Vertical,
    Horizontal
};

struct GridPoint
{
    int row;
    int col;
};
struct GridSize
{
    int rows;
    int cols;
};
struct PixelSize
{
    int width;
    int height;
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

struct CellProperty
{
    uint16_t hl_attrib_id;
    bool is_wide_char;
};

constexpr int MAX_HIGHLIGHT_ATTRIBS = 0xFFFF;
constexpr int MAX_CURSOR_MODE_INFOS = 64;
constexpr int MAX_FONT_LENGTH = 128;
constexpr float DEFAULT_DPI = 96.0f;
constexpr float POINTS_PER_INCH = 72.0f;
struct GlyphDrawingEffect;
struct GlyphRenderer;
struct Renderer
{
    CursorModeInfo cursor_mode_infos[MAX_CURSOR_MODE_INFOS] = {};
    Vec<HighlightAttributes> hl_attribs;
    Cursor cursor = {0};

    GlyphRenderer *glyph_renderer = nullptr;

    D3D_FEATURE_LEVEL d3d_feature_level;
    ID3D11Device2 *d3d_device = nullptr;
    ID3D11DeviceContext2 *d3d_context = nullptr;
    IDXGISwapChain2 *dxgi_swapchain = nullptr;
    HANDLE swapchain_wait_handle = nullptr;
    ID2D1Factory5 *d2d_factory = nullptr;
    ID2D1Device4 *d2d_device = nullptr;
    ID2D1DeviceContext4 *d2d_context = nullptr;
    ID2D1Bitmap1 *d2d_target_bitmap = nullptr;
    ID2D1SolidColorBrush *d2d_background_rect_brush = nullptr;

    IDWriteFontFace1 *font_face = nullptr;

    IDWriteFactory4 *dwrite_factory = nullptr;
    IDWriteTextFormat *dwrite_text_format = nullptr;

    bool disable_ligatures = false;
    IDWriteTypography *dwrite_typography = nullptr;

    float linespace_factor = 0;

    float last_requested_font_size = 0;
    wchar_t font[MAX_FONT_LENGTH] = {0};
    DWRITE_FONT_METRICS1 font_metrics = {};
    float dpi_scale = 0;
    float font_size = 0;
    float font_height = 0;
    float font_width = 0;
    float font_ascent = 0;
    float font_descent = 0;

    D2D1_SIZE_U pixel_size = {0};
    int grid_rows = 0;
    int grid_cols = 0;
    wchar_t *grid_chars = nullptr;
    CellProperty *grid_cell_properties = nullptr;

    HWND hwnd = nullptr;
    bool draw_active = false;
    bool ui_busy = false;

    Renderer(HWND hwnd, bool disable_ligatures, float linespace_factor,
             float monitor_dpi);
    ~Renderer();
    void Attach();
    void Resize(uint32_t width, uint32_t height);
    void UpdateGuiFont(const char *guifont, size_t strlen);
    void UpdateFont(float font_size, const char *font_string = "",
                    int strlen = 0);
    void Redraw(mpack_node_t params);
    PixelSize GridToPixelSize(int rows, int cols);
    GridSize PixelsToGridSize(int width, int height);
    GridSize GridSize()
    {
        return PixelsToGridSize(pixel_size.width, pixel_size.height);
    }
    GridPoint CursorToGridPoint(int x, int y);
};
