#pragma once
#include <stdint.h>
#include <wrl/client.h>
#include <memory>
#include <functional>
#include <list>
#include <vector>

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

enum class RendererEventTypes
{
    GridSizeChanged,
};
struct RendererEvent
{
    RendererEventTypes type;
    union
    {
        GridSize gridSize;
    };
};
using RendererEventCallback = std::function<void(const RendererEvent &)>;

class Renderer
{
    std::unique_ptr<class GridImpl> _grid;
    std::unique_ptr<class DeviceImpl> _device;
    std::unique_ptr<class SwapchainImpl> _swapchain;
    std::unique_ptr<class DWriteImpl> _dwrite;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> _d2d_target_bitmap;

    CursorModeInfo _cursor_mode_infos[MAX_CURSOR_MODE_INFOS] = {};
    Cursor _cursor = {};

    std::vector<HighlightAttributes> _hl_attribs;

    D2D1_SIZE_U _pixel_size = {0};

    HWND _hwnd = nullptr;
    bool _draw_active = false;
    bool _ui_busy = false;

    std::list<RendererEventCallback> _callbacks;

public:
    Renderer(HWND hwnd, bool disable_ligatures, float linespace_factor,
             uint32_t monitor_dpi, const RendererEventCallback &callback);
    ~Renderer();

    void Resize(uint32_t width, uint32_t height);
    void UpdateGuiFont(const char *guifont, size_t strlen);
    void UpdateFont(float font_size, const char *font_string, int strlen);
    void Redraw(mpack_node_t params);
    PixelSize GridToPixelSize(int rows, int cols);
    GridSize PixelsToGridSize(int width, int height);
    GridPoint CursorToGridPoint(int x, int y);
    GridSize GetGridSize();
    void SetDpiScale(uint32_t monitor_dpi);
    void ResizeFont(float size);
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

    void Flush();

private:
    void RaiseEvent(const RendererEvent &event);

    void InitializeWindowDependentResources();
    void HandleDeviceLost();
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
