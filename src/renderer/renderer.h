#pragma once
#include <wrl/client.h>

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
class Renderer
{
    CursorModeInfo _cursor_mode_infos[MAX_CURSOR_MODE_INFOS] = {};
    Cursor _cursor = {0};

    struct GlyphRenderer *_glyph_renderer = nullptr;

    D3D_FEATURE_LEVEL _d3d_feature_level;
    ID3D11Device2 *_d3d_device = nullptr;
    ID3D11DeviceContext2 *_d3d_context = nullptr;
    IDXGISwapChain2 *_dxgi_swapchain = nullptr;
    HANDLE _swapchain_wait_handle = nullptr;
    ID2D1Factory5 *_d2d_factory = nullptr;
    ID2D1Device4 *_d2d_device = nullptr;
    ID2D1Bitmap1 *_d2d_target_bitmap = nullptr;
    ID2D1SolidColorBrush *_d2d_background_rect_brush = nullptr;
    ID2D1SolidColorBrush *_drawing_effect_brush;
    ID2D1SolidColorBrush *_temp_brush;

    bool _disable_ligatures = false;
    IDWriteTypography *_dwrite_typography = nullptr;

    ID2D1DeviceContext4 *_d2d_context = nullptr;
    Vec<HighlightAttributes> _hl_attribs;
    float _last_requested_font_size = 0;
    IDWriteFactory4 *_dwrite_factory = nullptr;
    IDWriteTextFormat *_dwrite_text_format = nullptr;
    IDWriteFontFace1 *_font_face = nullptr;
    float _linespace_factor = 0;
    wchar_t _font[MAX_FONT_LENGTH] = {0};
    DWRITE_FONT_METRICS1 _font_metrics = {};
    float _dpi_scale = 0;
    float _font_size = 0;
    float _font_height = 0;
    float _font_width = 0;
    float _font_ascent = 0;
    float _font_descent = 0;

    D2D1_SIZE_U _pixel_size = {0};
    int _grid_rows = 0;
    int _grid_cols = 0;
    wchar_t *_grid_chars = nullptr;
    CellProperty *_grid_cell_properties = nullptr;

    HWND _hwnd = nullptr;
    bool _draw_active = false;
    bool _ui_busy = false;

public:
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
    GridPoint CursorToGridPoint(int x, int y);
    GridSize GridSize();
    bool SetDpiScale(float current_dpi, int *pRows, int *pCols);
    bool ResizeFont(float size, int *pRows, int *pCols);
    HRESULT
    DrawGlyphRun(float baseline_origin_x, float baseline_origin_y,
                 DWRITE_MEASURING_MODE measuring_mode,
                 DWRITE_GLYPH_RUN const *glyph_run,
                 DWRITE_GLYPH_RUN_DESCRIPTION const *glyph_run_description,
                 IUnknown *client_drawing_effect);
    HRESULT DrawUnderline(float baseline_origin_x, float baseline_origin_y,
                          DWRITE_UNDERLINE const *underline,
                          IUnknown *client_drawing_effect);
    HRESULT GetCurrentTransform(DWRITE_MATRIX *transform);

private:
    void InitializeD2D();
    void InitializeD3D();
    void InitializeDWrite();
    void InitializeWindowDependentResources();
    void HandleDeviceLost();
    void CopyFrontToBack();
    float GetTextWidth(wchar_t *text, uint32_t length);
    void UpdateFontMetrics(float font_size, const char *font_string,
                           int strlen);
    void UpdateDefaultColors(mpack_node_t default_colors);
    void UpdateHighlightAttributes(mpack_node_t highlight_attribs);
    uint32_t CreateForegroundColor(HighlightAttributes *hl_attribs);
    uint32_t CreateBackgroundColor(HighlightAttributes *hl_attribs);
    uint32_t CreateSpecialColor(HighlightAttributes *hl_attribs);
    void ApplyHighlightAttributes(HighlightAttributes *hl_attribs,
                                  IDWriteTextLayout *text_layout, int start,
                                  int end);
    D2D1_RECT_F GetCursorForegroundRect(D2D1_RECT_F cursor_bg_rect);
    void DrawBackgroundRect(D2D1_RECT_F rect, HighlightAttributes *hl_attribs);
    void DrawHighlightedText(D2D1_RECT_F rect, wchar_t *text, uint32_t length,
                             HighlightAttributes *hl_attribs);
    void DrawGridLine(int row);
    void DrawCursor();
    void DrawBorderRectangles();
    void DrawGridLines(mpack_node_t grid_lines);
    void UpdateGridSize(mpack_node_t grid_resize);
    void UpdateCursorPos(mpack_node_t cursor_goto);
    void UpdateCursorMode(mpack_node_t mode_change);
    void UpdateCursorModeInfos(mpack_node_t mode_info_set_params);
    void ScrollRegion(mpack_node_t scroll_region);
    void SetGuiOptions(mpack_node_t option_set);
    void ClearGrid();
    void StartDraw();
    void FinishDraw();
};
