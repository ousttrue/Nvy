#pragma once
#include <d2d1_3.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <memory>
#include "cursor.h"

constexpr const char *DEFAULT_FONT = "Consolas";
constexpr float DEFAULT_FONT_SIZE = 14.0f;

struct PixelSize
{
    int width;
    int height;
};

constexpr int MAX_FONT_LENGTH = 128;
constexpr float DEFAULT_DPI = 96.0f;
constexpr float POINTS_PER_INCH = 72.0f;

struct HighlightAttribute;
class Grid;
class Renderer
{
    std::unique_ptr<class DeviceImpl> _device;
    std::unique_ptr<class SwapchainImpl> _swapchain;
    std::unique_ptr<class DWriteImpl> _dwrite;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> _d2d_target_bitmap;

    D2D1_SIZE_U _pixel_size = {0};

    HWND _hwnd = nullptr;
    bool _draw_active = false;

    const HighlightAttribute *_defaultHL = nullptr;

public:
    Renderer(HWND hwnd, bool disable_ligatures, float linespace_factor,
             float monitor_dpi, const HighlightAttribute *defaultHL);
    ~Renderer();
    // backbuffer
    D2D1_SIZE_U Size() const
    {
        return _pixel_size;
    }
    void Attach();
    void Resize(uint32_t width, uint32_t height);
    // font
    D2D1_SIZE_U FontSize() const;
    void UpdateGuiFont(const char *guifont, size_t strlen);
    void UpdateFont(float font_size, const char *font_string, int strlen);
    PixelSize GridToPixelSize(int rows, int cols);
    D2D1_SIZE_U SetDpiScale(float current_dpi);
    D2D1_SIZE_U ResizeFont(float size);
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

    // private:
    void InitializeWindowDependentResources();
    void HandleDeviceLost();
    void ApplyHighlightAttributes(IDWriteTextLayout *text_layout, int start,
                                  int end,
                                  const HighlightAttribute *hl_attribs);
    D2D1_RECT_F GetCursorForegroundRect(D2D1_RECT_F cursor_bg_rect,
                                        CursorShape shape);
    void DrawBackgroundRect(int rows, int cols, const HighlightAttribute *hl);
    void DrawBackgroundRect(D2D1_RECT_F rect,
                            const HighlightAttribute *hl_attribs);
    void DrawHighlightedText(D2D1_RECT_F rect, const wchar_t *text,
                             uint32_t length,
                             const HighlightAttribute *hl_attribs);
    void DrawGridLine(const Grid *grid, int row);
    void DrawCursor(const Grid *grid);
    void DrawBorderRectangles(const Grid *grid);
    void StartDraw();
    void FinishDraw();
};
