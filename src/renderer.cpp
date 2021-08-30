#include "renderer.h"
#include "dx_helper.h"
#include "nvim_grid.h"
#include <algorithm>
#include <assert.h>
#include <d2d1_3.h>
#include <d3d11_4.h>
#include <dwmapi.h>
#include <dwrite_3.h>
#include <shellscalingapi.h>
#include <tuple>
#include <vector>
#include <wrl/client.h>

using namespace Microsoft::WRL;

constexpr const char *DEFAULT_FONT = "Consolas";
constexpr float DEFAULT_FONT_SIZE = 14.0f;

constexpr int MAX_FONT_LENGTH = 128;
constexpr float DEFAULT_DPI = 96.0f;
constexpr float POINTS_PER_INCH = 72.0f;

struct DECLSPEC_UUID("8d4d2884-e4d9-11ea-87d0-0242ac130003") GlyphDrawingEffect
    : public IUnknown {
  ULONG _ref_count;
  uint32_t _text_color;
  uint32_t _special_color;

private:
  GlyphDrawingEffect(uint32_t text_color, uint32_t special_color)
      : _ref_count(1), _text_color(text_color), _special_color(special_color) {}

public:
  inline ULONG AddRef() noexcept override {
    return InterlockedIncrement(&_ref_count);
  }

  inline ULONG Release() noexcept override {
    ULONG new_count = InterlockedDecrement(&_ref_count);
    if (new_count == 0) {
      delete this;
      return 0;
    }
    return new_count;
  }

  HRESULT QueryInterface(REFIID riid, void **ppv_object) noexcept override {
    if (__uuidof(GlyphDrawingEffect) == riid) {
      *ppv_object = this;
    } else if (__uuidof(IUnknown) == riid) {
      *ppv_object = this;
    } else {
      *ppv_object = nullptr;
      return E_FAIL;
    }

    this->AddRef();
    return S_OK;
  }

  static HRESULT Create(uint32_t text_color, uint32_t special_color,
                        GlyphDrawingEffect **pp) {
    auto p = new GlyphDrawingEffect(text_color, special_color);
    *pp = p;
    return S_OK;
  }
};

struct GlyphRenderer : public IDWriteTextRenderer {
  ULONG _ref_count;

private:
  GlyphRenderer() : _ref_count(1) {}

public:
  static HRESULT Create(GlyphRenderer **pp) {
    auto p = new GlyphRenderer();
    *pp = p;
    return S_OK;
  }

  HRESULT
  DrawGlyphRun(void *client_drawing_context, float baseline_origin_x,
               float baseline_origin_y, DWRITE_MEASURING_MODE measuring_mode,
               DWRITE_GLYPH_RUN const *glyph_run,
               DWRITE_GLYPH_RUN_DESCRIPTION const *glyph_run_description,
               IUnknown *client_drawing_effect) noexcept override;

  HRESULT DrawInlineObject(void *client_drawing_context, float origin_x,
                           float origin_y, IDWriteInlineObject *inline_obj,
                           BOOL is_sideways, BOOL is_right_to_left,
                           IUnknown *client_drawing_effect) noexcept override {
    return E_NOTIMPL;
  }

  HRESULT
  DrawStrikethrough(void *client_drawing_context, float baseline_origin_x,
                    float baseline_origin_y,
                    DWRITE_STRIKETHROUGH const *strikethrough,
                    IUnknown *client_drawing_effect) noexcept override {
    return E_NOTIMPL;
  }

  HRESULT DrawUnderline(void *client_drawing_context, float baseline_origin_x,
                        float baseline_origin_y,
                        DWRITE_UNDERLINE const *underline,
                        IUnknown *client_drawing_effect) noexcept override;

  HRESULT IsPixelSnappingDisabled(void *client_drawing_context,
                                  BOOL *is_disabled) noexcept override {
    *is_disabled = false;
    return S_OK;
  }

  HRESULT GetCurrentTransform(void *client_drawing_context,
                              DWRITE_MATRIX *transform) noexcept override;

  HRESULT GetPixelsPerDip(void *client_drawing_context,
                          float *pixels_per_dip) noexcept override {
    *pixels_per_dip = 1.0f;
    return S_OK;
  }

  ULONG AddRef() noexcept override { return InterlockedIncrement(&_ref_count); }

  ULONG Release() noexcept override {
    ULONG new_count = InterlockedDecrement(&_ref_count);
    if (new_count == 0) {
      delete this;
      return 0;
    }
    return new_count;
  }

  HRESULT QueryInterface(REFIID riid, void **ppv_object) noexcept override {
    if (__uuidof(IDWriteTextRenderer) == riid) {
      *ppv_object = this;
    } else if (__uuidof(IDWritePixelSnapping) == riid) {
      *ppv_object = this;
    } else if (__uuidof(IUnknown) == riid) {
      *ppv_object = this;
    } else {
      *ppv_object = nullptr;
      return E_FAIL;
    }

    this->AddRef();
    return S_OK;
  }
};

class DWriteImpl {
public:
  ComPtr<IDWriteFactory4> _dwrite_factory;

private:
  bool _disable_ligatures = false;
  ComPtr<IDWriteTypography> _dwrite_typography;
  ComPtr<IDWriteTextFormat> _dwrite_text_format;

  float _last_requested_font_size = 0;
  wchar_t _font[MAX_FONT_LENGTH] = {0};
  ComPtr<IDWriteFontFace1> _font_face;
  DWRITE_FONT_METRICS1 _font_metrics = {};
  float _dpi_scale = 0;

public:
  float _font_size = 0;
  float _font_height = 0;
  float _font_width = 0;
  float _font_ascent = 0;
  float _font_descent = 0;
  float _linespace_factor = 0;

public:
  DWriteImpl() {}
  ~DWriteImpl() {}

  static std::unique_ptr<DWriteImpl>
  Create(bool disable_ligatures, float linespace_factor, float monitor_dpi) {
    ComPtr<IDWriteFactory4> dwrite_factory;
    auto hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory4),
        reinterpret_cast<IUnknown **>(dwrite_factory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
      return nullptr;
    }

    ComPtr<IDWriteTypography> dwrite_typography;
    if (disable_ligatures) {
      hr = dwrite_factory->CreateTypography(&dwrite_typography);
      if (FAILED(hr)) {
        return nullptr;
      }

      hr = dwrite_typography->AddFontFeature(DWRITE_FONT_FEATURE{
          .nameTag = DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES,
          .parameter = 0});
      if (FAILED(hr)) {
        return nullptr;
      }
    }

    auto p = std::unique_ptr<DWriteImpl>(new DWriteImpl());
    p->_disable_ligatures = disable_ligatures;
    p->_linespace_factor = linespace_factor;
    p->_dpi_scale = monitor_dpi / 96.0f;
    p->_dwrite_factory = dwrite_factory;
    p->_dwrite_typography = dwrite_typography;
    return p;
  }

  void SetDpiScale(float current_dpi) {
    _dpi_scale = current_dpi / 96.0f;
    UpdateFont(_last_requested_font_size);
  }

  void ResizeFont(float size) { UpdateFont(_last_requested_font_size + size); }

  void UpdateFont(float font_size, const char *font_string = "",
                  int strlen = 0) {
    this->_dwrite_text_format.Reset();
    this->UpdateFontMetrics(font_size, font_string, strlen);
  }

  void UpdateFontMetrics(float font_size, const char *font_string, int strlen) {
    font_size = std::max(5.0f, std::min(font_size, 150.0f));
    this->_last_requested_font_size = font_size;

    ComPtr<IDWriteFontCollection> font_collection;
    WIN_CHECK(this->_dwrite_factory->GetSystemFontCollection(&font_collection));

    int wstrlen = MultiByteToWideChar(CP_UTF8, 0, font_string, strlen, 0, 0);
    if (wstrlen != 0 && wstrlen < MAX_FONT_LENGTH) {
      MultiByteToWideChar(CP_UTF8, 0, font_string, strlen, this->_font,
                          MAX_FONT_LENGTH - 1);
      this->_font[wstrlen] = L'\0';
    }

    uint32_t index;
    BOOL exists;
    font_collection->FindFamilyName(this->_font, &index, &exists);

    const wchar_t *fallback_font = L"Consolas";
    if (!exists) {
      font_collection->FindFamilyName(fallback_font, &index, &exists);
      memcpy(this->_font, fallback_font,
             (wcslen(fallback_font) + 1) * sizeof(wchar_t));
    }

    ComPtr<IDWriteFontFamily> font_family;
    WIN_CHECK(font_collection->GetFontFamily(index, &font_family));

    ComPtr<IDWriteFont> write_font;
    WIN_CHECK(font_family->GetFirstMatchingFont(
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, &write_font));

    ComPtr<IDWriteFontFace> font_face;
    WIN_CHECK(write_font->CreateFontFace(&font_face));
    WIN_CHECK(font_face->QueryInterface<IDWriteFontFace1>(&this->_font_face));

    this->_font_face->GetMetrics(&this->_font_metrics);

    uint16_t glyph_index;
    constexpr uint32_t codepoint = L'A';
    WIN_CHECK(this->_font_face->GetGlyphIndicesW(&codepoint, 1, &glyph_index));

    int32_t glyph_advance_in_em;
    WIN_CHECK(this->_font_face->GetDesignGlyphAdvances(1, &glyph_index,
                                                       &glyph_advance_in_em));

    float desired_height =
        font_size * this->_dpi_scale * (DEFAULT_DPI / POINTS_PER_INCH);
    float width_advance = static_cast<float>(glyph_advance_in_em) /
                          this->_font_metrics.designUnitsPerEm;
    float desired_width = desired_height * width_advance;

    // We need the width to be aligned on a per-pixel boundary, thus we will
    // roundf the desired_width and calculate the font size given the new
    // exact width
    this->_font_width = roundf(desired_width);
    this->_font_size = this->_font_width / width_advance;
    float frac_font_ascent = (this->_font_size * this->_font_metrics.ascent) /
                             this->_font_metrics.designUnitsPerEm;
    float frac_font_descent = (this->_font_size * this->_font_metrics.descent) /
                              this->_font_metrics.designUnitsPerEm;
    float linegap = (this->_font_size * this->_font_metrics.lineGap) /
                    this->_font_metrics.designUnitsPerEm;
    float half_linegap = linegap / 2.0f;
    this->_font_ascent = ceilf(frac_font_ascent + half_linegap);
    this->_font_descent = ceilf(frac_font_descent + half_linegap);
    this->_font_height = this->_font_ascent + this->_font_descent;
    this->_font_height *= this->_linespace_factor;

    WIN_CHECK(this->_dwrite_factory->CreateTextFormat(
        this->_font, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, this->_font_size,
        L"en-us", &this->_dwrite_text_format));

    WIN_CHECK(this->_dwrite_text_format->SetLineSpacing(
        DWRITE_LINE_SPACING_METHOD_UNIFORM, this->_font_height,
        this->_font_ascent * this->_linespace_factor));
    WIN_CHECK(this->_dwrite_text_format->SetParagraphAlignment(
        DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    WIN_CHECK(this->_dwrite_text_format->SetWordWrapping(
        DWRITE_WORD_WRAPPING_NO_WRAP));
  }

  float GetTextWidth(const wchar_t *text, uint32_t length) {
    // Create dummy text format to hit test the width of the font
    ComPtr<IDWriteTextLayout> test_text_layout;
    WIN_CHECK(this->_dwrite_factory->CreateTextLayout(
        text, length, this->_dwrite_text_format.Get(), 0.0f, 0.0f,
        &test_text_layout));

    DWRITE_HIT_TEST_METRICS metrics;
    float _;
    WIN_CHECK(test_text_layout->HitTestTextPosition(0, 0, &_, &_, &metrics));
    return metrics.width;
  }

  ComPtr<IDWriteTextLayout1>
  GetTextLayout(const D2D1_RECT_F &rect, const wchar_t *text, uint32_t length) {
    ComPtr<IDWriteTextLayout> temp_text_layout;
    WIN_CHECK(this->_dwrite_factory->CreateTextLayout(
        text, length, this->_dwrite_text_format.Get(), rect.right - rect.left,
        rect.bottom - rect.top, &temp_text_layout));
    ComPtr<IDWriteTextLayout1> text_layout;
    temp_text_layout.As(&text_layout);
    return text_layout;
  }

  void SetTypographyIfNotLigatures(const ComPtr<IDWriteTextLayout> &text_layout,
                                   uint32_t length) {
    if (this->_disable_ligatures) {
      text_layout->SetTypography(
          this->_dwrite_typography.Get(),
          DWRITE_TEXT_RANGE{.startPosition = 0, .length = length});
    }
  }
};

class DeviceImpl {
public:
  ComPtr<ID3D11Device2> _d3d_device;
  ComPtr<ID3D11DeviceContext2> _d3d_context;

  ComPtr<ID2D1Factory5> _d2d_factory;
  ComPtr<ID2D1Device4> _d2d_device;
  ComPtr<ID2D1DeviceContext4> _d2d_context;
  ComPtr<ID2D1SolidColorBrush> _d2d_background_rect_brush;
  ComPtr<ID2D1SolidColorBrush> _drawing_effect_brush;
  ComPtr<ID2D1SolidColorBrush> _temp_brush;

  ComPtr<GlyphRenderer> _glyph_renderer;

public:
  static std::unique_ptr<DeviceImpl> Create() {
    uint32_t flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Force DirectX 11.1
    D3D_FEATURE_LEVEL d3d_feature_level;
    ComPtr<ID3D11Device> temp_device;
    ComPtr<ID3D11DeviceContext> temp_context;
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1};
    WIN_CHECK(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, feature_levels,
        ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &temp_device,
        &d3d_feature_level, &temp_context));

    auto p = std::unique_ptr<DeviceImpl>(new DeviceImpl);

    WIN_CHECK(temp_device.As(&p->_d3d_device));
    WIN_CHECK(temp_context.As(&p->_d3d_context));

    D2D1_FACTORY_OPTIONS options{};
#ifndef NDEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    WIN_CHECK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options,
                                p->_d2d_factory.ReleaseAndGetAddressOf()));

    ComPtr<IDXGIDevice3> dxgi_device;
    WIN_CHECK(p->_d3d_device.As(&dxgi_device));
    WIN_CHECK(
        p->_d2d_factory->CreateDevice(dxgi_device.Get(), &p->_d2d_device));
    WIN_CHECK(p->_d2d_device->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS,
        &p->_d2d_context));

    WIN_CHECK(p->_d2d_context->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &p->_d2d_background_rect_brush));
    WIN_CHECK(p->_d2d_context->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &p->_drawing_effect_brush));
    WIN_CHECK(p->_d2d_context->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &p->_temp_brush));

    auto hr = GlyphRenderer::Create(&p->_glyph_renderer);
    if (FAILED(hr)) {
      assert(false);
    }

    return p;
  }
};

class SwapchainImpl {
  ComPtr<IDXGISwapChain2> _dxgi_swapchain;
  HANDLE _swapchain_wait_handle = nullptr;
  DXGI_SWAP_CHAIN_DESC _desc;

public:
  static std::unique_ptr<SwapchainImpl>
  Create(const ComPtr<ID3D11Device2> &d3d_device, HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc{
        .Width = 0,
        .Height = 0,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .Scaling = DXGI_SCALING_NONE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
        .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
        .Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
                 DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING};

    ComPtr<IDXGIDevice3> dxgi_device;
    WIN_CHECK(d3d_device.As(&dxgi_device));
    ComPtr<IDXGIAdapter> dxgi_adapter;
    WIN_CHECK(dxgi_device->GetAdapter(&dxgi_adapter));
    ComPtr<IDXGIFactory2> dxgi_factory;
    WIN_CHECK(dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)));

    ComPtr<IDXGISwapChain1> dxgi_swapchain_temp;
    WIN_CHECK(dxgi_factory->CreateSwapChainForHwnd(
        d3d_device.Get(), hwnd, &swapchain_desc, nullptr, nullptr,
        &dxgi_swapchain_temp));
    WIN_CHECK(dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    ComPtr<IDXGISwapChain2> dxgi_swapchain;
    WIN_CHECK(dxgi_swapchain_temp.As(&dxgi_swapchain));
    WIN_CHECK(dxgi_swapchain->SetMaximumFrameLatency(1));

    auto p = std::unique_ptr<SwapchainImpl>(new SwapchainImpl);
    p->_dxgi_swapchain = dxgi_swapchain;
    p->_swapchain_wait_handle =
        p->_dxgi_swapchain->GetFrameLatencyWaitableObject();
    p->_dxgi_swapchain->GetDesc(&p->_desc);

    return p;
  }

  void Wait() { WaitForSingleObjectEx(_swapchain_wait_handle, 1000, true); }

  std::tuple<uint32_t, uint32_t> GetSize() const {
    return std::make_pair(_desc.BufferDesc.Width, _desc.BufferDesc.Height);
  }

  HRESULT Resize(uint32_t w, uint32_t h) {
    DXGI_SWAP_CHAIN_DESC desc;
    _dxgi_swapchain->GetDesc(&desc);
    if (desc.BufferDesc.Width == w && desc.BufferDesc.Height == h) {
      return S_OK;
    }

    HRESULT hr = this->_dxgi_swapchain->ResizeBuffers(
        2, w, h, DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
            DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
    return hr;
  }

  ComPtr<IDXGISurface2> GetBackbuffer() {
    ComPtr<IDXGISurface2> dxgi_backbuffer;
    WIN_CHECK(
        this->_dxgi_swapchain->GetBuffer(0, IID_PPV_ARGS(&dxgi_backbuffer)));
    return dxgi_backbuffer;
  }

  HRESULT
  PresentCopyFrontToBack(const ComPtr<ID3D11DeviceContext2> &d3d_context) {
    HRESULT hr = this->_dxgi_swapchain->Present(0, 0);
    if (FAILED(hr)) {
      return hr;
    }

    ComPtr<ID3D11Resource> back;
    WIN_CHECK(this->_dxgi_swapchain->GetBuffer(0, IID_PPV_ARGS(&back)));
    ComPtr<ID3D11Resource> front;
    WIN_CHECK(this->_dxgi_swapchain->GetBuffer(1, IID_PPV_ARGS(&front)));
    d3d_context->CopyResource(back.Get(), front.Get());
    return S_OK;
  }
};

static UINT GetMonitorDpi(HWND hwnd) {
  RECT window_rect;
  DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect,
                        sizeof(RECT));
  HMONITOR monitor = MonitorFromPoint({window_rect.left, window_rect.top},
                                      MONITOR_DEFAULTTONEAREST);

  UINT dpi = 0;
  GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi, &dpi);
  return dpi;
}

class RendererImpl {
  std::unique_ptr<class DeviceImpl> _device;
  std::unique_ptr<class SwapchainImpl> _swapchain;
  std::unique_ptr<class DWriteImpl> _dwrite;
  Microsoft::WRL::ComPtr<ID2D1Bitmap1> _d2d_target_bitmap;

  HWND _hwnd = nullptr;
  bool _draw_active = false;

  const HighlightAttribute *_defaultHL = nullptr;

  D2D1_SIZE_U _pixel_size = {0};
  GridSize _grid_size = {};
  on_rows_cols_t _on_rows_cols;

public:
  RendererImpl(HWND hwnd, bool disable_ligatures, float linespace_factor,
               const HighlightAttribute *defaultHL)
      : _dwrite(DWriteImpl::Create(disable_ligatures, linespace_factor,
                                   GetMonitorDpi(hwnd))),
        _defaultHL(defaultHL) {
    this->_hwnd = hwnd;
    this->HandleDeviceLost();
  }

  void InitializeWindowDependentResources() {
    if (this->_swapchain) {
      uint32_t w, h;
      std::tie(w, h) = this->_swapchain->GetSize();
      if (_pixel_size.width == w && _pixel_size.height == h) {
        // not resized. use same bitmap
        return;
      }

      _d2d_target_bitmap = nullptr;
      HRESULT hr =
          this->_swapchain->Resize(_pixel_size.width, _pixel_size.height);
      if (hr == DXGI_ERROR_DEVICE_REMOVED) {
        this->HandleDeviceLost();
      }
    } else {
      this->_swapchain = SwapchainImpl::Create(_device->_d3d_device, _hwnd);
    }

    auto dxgi_backbuffer = _swapchain->GetBackbuffer();

    constexpr D2D1_BITMAP_PROPERTIES1 target_bitmap_properties{
        .pixelFormat = D2D1_PIXEL_FORMAT{.format = DXGI_FORMAT_B8G8R8A8_UNORM,
                                         .alphaMode = D2D1_ALPHA_MODE_IGNORE},
        .dpiX = DEFAULT_DPI,
        .dpiY = DEFAULT_DPI,
        .bitmapOptions =
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW};
    WIN_CHECK(_device->_d2d_context->CreateBitmapFromDxgiSurface(
        dxgi_backbuffer.Get(), &target_bitmap_properties,
        &this->_d2d_target_bitmap));
    _device->_d2d_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
  }

  void HandleDeviceLost() {
    _device.reset();
    _swapchain.reset();
    _d2d_target_bitmap.Reset();

    _device = DeviceImpl::Create();
    this->UpdateFont(DEFAULT_FONT_SIZE, DEFAULT_FONT,
                     static_cast<int>(strlen(DEFAULT_FONT)));
  }

  void Resize(uint32_t width, uint32_t height) {
    _pixel_size.width = width;
    _pixel_size.height = height;
    UpdateSize();
  }

  D2D1_SIZE_U Size() const { return _pixel_size; }

  void UpdateSize() {
    auto fontSize = FontSize();
    auto gridSize = GridSize::FromWindowSize(
        _pixel_size.width, _pixel_size.height, fontSize.width, fontSize.height);
    SetGridSize(gridSize.rows, gridSize.cols);
  }

  D2D1_SIZE_U FontSize() const {
    return {
        .width = static_cast<UINT>(_dwrite->_font_width),
        .height = static_cast<UINT>(_dwrite->_font_height),
    };
  }

  void UpdateFont(float font_size, const char *font_string, int strlen) {
    _dwrite->UpdateFont(font_size, font_string, strlen);
    UpdateSize();
  }

  void SetGridSize(int rows, int cols) {
    if (rows == _grid_size.rows && cols == _grid_size.cols) {
      return;
    }
    _grid_size.rows = rows;
    _grid_size.cols = cols;
    _on_rows_cols(_grid_size.rows, _grid_size.cols);
  }

  void ApplyHighlightAttributes(IDWriteTextLayout *text_layout, int start,
                                int end, const HighlightAttribute *hl_attribs) {
    ComPtr<GlyphDrawingEffect> drawing_effect;
    GlyphDrawingEffect::Create(hl_attribs->CreateForegroundColor(),
                               hl_attribs->CreateSpecialColor(),
                               &drawing_effect);
    DWRITE_TEXT_RANGE range{.startPosition = static_cast<uint32_t>(start),
                            .length = static_cast<uint32_t>(end - start)};
    if (hl_attribs->flags & HL_ATTRIB_ITALIC) {
      text_layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
    }
    if (hl_attribs->flags & HL_ATTRIB_BOLD) {
      text_layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
    }
    if (hl_attribs->flags & HL_ATTRIB_STRIKETHROUGH) {
      text_layout->SetStrikethrough(true, range);
    }
    if (hl_attribs->flags & HL_ATTRIB_UNDERLINE) {
      text_layout->SetUnderline(true, range);
    }
    if (hl_attribs->flags & HL_ATTRIB_UNDERCURL) {
      text_layout->SetUnderline(true, range);
    }
    text_layout->SetDrawingEffect(drawing_effect.Get(), range);
  }

  void DrawBackgroundRect(D2D1_RECT_F rect,
                          const HighlightAttribute *hl_attribs) {
    auto color = hl_attribs->CreateBackgroundColor();
    _device->_d2d_background_rect_brush->SetColor(D2D1::ColorF(color));
    _device->_d2d_context->FillRectangle(
        rect, _device->_d2d_background_rect_brush.Get());
  }

  D2D1_RECT_F GetCursorForegroundRect(D2D1_RECT_F cursor_bg_rect,
                                      CursorShape shape) {
    switch (shape) {
    case CursorShape::None: {
      return cursor_bg_rect;
    }
    case CursorShape::Block: {
      return cursor_bg_rect;
    }
    case CursorShape::Vertical: {
      cursor_bg_rect.right = cursor_bg_rect.left + 2;
      return cursor_bg_rect;
    }
    case CursorShape::Horizontal: {
      cursor_bg_rect.top = cursor_bg_rect.bottom - 2;
      return cursor_bg_rect;
    }
    }
    return cursor_bg_rect;
  }

  void DrawHighlightedText(D2D1_RECT_F rect, const wchar_t *text,
                           uint32_t length,
                           const HighlightAttribute *hl_attribs) {
    auto text_layout = _dwrite->GetTextLayout(rect, text, length);
    this->ApplyHighlightAttributes(text_layout.Get(), 0, 1, hl_attribs);

    _device->_d2d_context->PushAxisAlignedClip(rect,
                                               D2D1_ANTIALIAS_MODE_ALIASED);
    text_layout->Draw(this, _device->_glyph_renderer.Get(), rect.left,
                      rect.top);
    _device->_d2d_context->PopAxisAlignedClip();
  }

  void DrawGridLine(const NvimGrid *grid, int row) {
    auto cols = grid->Cols();
    int base = row * cols;

    D2D1_RECT_F rect{.left = 0.0f,
                     .top = row * _dwrite->_font_height,
                     .right = cols * _dwrite->_font_width,
                     .bottom =
                         (row * _dwrite->_font_height) + _dwrite->_font_height};

    auto text_layout = _dwrite->GetTextLayout(rect, &grid->Chars()[base], cols);

    uint16_t hl_attrib_id = grid->Props()[base].hl_attrib_id;
    int col_offset = 0;
    for (int i = 0; i < cols; ++i) {
      // Add spacing for wide chars
      if (grid->Props()[base + i].is_wide_char) {
        float char_width = _dwrite->GetTextWidth(&grid->Chars()[base + i], 2);
        DWRITE_TEXT_RANGE range{.startPosition = static_cast<uint32_t>(i),
                                .length = 1};
        text_layout->SetCharacterSpacing(
            0, (_dwrite->_font_width * 2) - char_width, 0, range);
      }

      // Add spacing for unicode chars. These characters are still single char
      // width, but some of them by default will take up a bit more or less,
      // leading to issues. So we realign them here.
      else if (grid->Chars()[base + i] > 0xFF) {
        float char_width = _dwrite->GetTextWidth(&grid->Chars()[base + i], 1);
        if (abs(char_width - _dwrite->_font_width) > 0.01f) {
          DWRITE_TEXT_RANGE range{.startPosition = static_cast<uint32_t>(i),
                                  .length = 1};
          text_layout->SetCharacterSpacing(0, _dwrite->_font_width - char_width,
                                           0, range);
        }
      }

      // Check if the attributes change,
      // if so draw until this point and continue with the new attributes
      if (grid->Props()[base + i].hl_attrib_id != hl_attrib_id) {
        D2D1_RECT_F bg_rect{.left = col_offset * _dwrite->_font_width,
                            .top = row * _dwrite->_font_height,
                            .right = col_offset * _dwrite->_font_width +
                                     _dwrite->_font_width * (i - col_offset),
                            .bottom = (row * _dwrite->_font_height) +
                                      _dwrite->_font_height};
        this->DrawBackgroundRect(bg_rect, &grid->hl(hl_attrib_id));
        this->ApplyHighlightAttributes(text_layout.Get(), col_offset, i,
                                       &grid->hl(hl_attrib_id));

        hl_attrib_id = grid->Props()[base + i].hl_attrib_id;
        col_offset = i;
      }
    }

    // Draw the remaining columns, there is always atleast the last column to
    // draw, but potentially more in case the last X columns share the same
    // hl_attrib
    D2D1_RECT_F last_rect = rect;
    last_rect.left = col_offset * _dwrite->_font_width;
    this->DrawBackgroundRect(last_rect, &grid->hl(hl_attrib_id));
    this->ApplyHighlightAttributes(text_layout.Get(), col_offset, cols,
                                   &grid->hl(hl_attrib_id));

    _device->_d2d_context->PushAxisAlignedClip(rect,
                                               D2D1_ANTIALIAS_MODE_ALIASED);
    _dwrite->SetTypographyIfNotLigatures(text_layout,
                                         static_cast<uint32_t>(cols));
    text_layout->Draw(this, _device->_glyph_renderer.Get(), 0.0f, rect.top);
    _device->_d2d_context->PopAxisAlignedClip();
  }

  void DrawCursor(const NvimGrid *grid) {
    int cursor_grid_offset = grid->CursorOffset();

    int double_width_char_factor = 1;
    if (cursor_grid_offset < grid->Count() &&
        grid->Props()[cursor_grid_offset].is_wide_char) {
      double_width_char_factor += 1;
    }

    auto cursor_hl_attribs = grid->hl(grid->CursorModeHighlightAttribute());
    if (grid->CursorModeHighlightAttribute() == 0) {
      cursor_hl_attribs.flags ^= HL_ATTRIB_REVERSE;
    }

    D2D1_RECT_F cursor_rect{
        .left = grid->CursorCol() * _dwrite->_font_width,
        .top = grid->CursorRow() * _dwrite->_font_height,
        .right = grid->CursorCol() * _dwrite->_font_width +
                 _dwrite->_font_width * double_width_char_factor,
        .bottom = (grid->CursorRow() * _dwrite->_font_height) +
                  _dwrite->_font_height};
    D2D1_RECT_F cursor_fg_rect =
        this->GetCursorForegroundRect(cursor_rect, grid->GetCursorShape());
    this->DrawBackgroundRect(cursor_fg_rect, &cursor_hl_attribs);

    if (grid->GetCursorShape() == CursorShape::Block) {
      this->DrawHighlightedText(cursor_fg_rect,
                                &grid->Chars()[cursor_grid_offset],
                                double_width_char_factor, &cursor_hl_attribs);
    }
  }

  void DrawBorderRectangles(const NvimGrid *grid) {
    float left_border = _dwrite->_font_width * grid->Cols();
    float top_border = _dwrite->_font_height * grid->Rows();

    if (left_border != static_cast<float>(this->_pixel_size.width)) {
      D2D1_RECT_F vertical_rect{
          .left = left_border,
          .top = 0.0f,
          .right = static_cast<float>(this->_pixel_size.width),
          .bottom = static_cast<float>(this->_pixel_size.height)};
      this->DrawBackgroundRect(vertical_rect, &grid->hl(0));
    }

    if (top_border != static_cast<float>(this->_pixel_size.height)) {
      D2D1_RECT_F horizontal_rect{
          .left = 0.0f,
          .top = top_border,
          .right = static_cast<float>(this->_pixel_size.width),
          .bottom = static_cast<float>(this->_pixel_size.height)};
      this->DrawBackgroundRect(horizontal_rect, &grid->hl(0));
    }
  }

  void DrawBackgroundRect(int rows, int cols, const HighlightAttribute *hl) {
    D2D1_RECT_F rect{.left = 0.0f,
                     .top = 0.0f,
                     .right = cols * _dwrite->_font_width,
                     .bottom = rows * _dwrite->_font_height};
    this->DrawBackgroundRect(rect, hl);
  }

  void StartDraw() {
    InitializeWindowDependentResources();
    if (!this->_draw_active) {
      _swapchain->Wait();

      _device->_d2d_context->SetTarget(this->_d2d_target_bitmap.Get());
      _device->_d2d_context->BeginDraw();
      _device->_d2d_context->SetTransform(D2D1::IdentityMatrix());
      this->_draw_active = true;
    }
  }

  void FinishDraw() {
    _device->_d2d_context->EndDraw();

    auto hr = _swapchain->PresentCopyFrontToBack(_device->_d3d_context);

    this->_draw_active = false;

    // clear render target
    ID3D11RenderTargetView *null_views[] = {nullptr};
    _device->_d3d_context->OMSetRenderTargets(ARRAYSIZE(null_views), null_views,
                                              nullptr);
    _device->_d2d_context->SetTarget(nullptr);
    _device->_d3d_context->Flush();

    if (hr == DXGI_ERROR_DEVICE_REMOVED) {
      this->HandleDeviceLost();
    }
  }

  D2D1_SIZE_U GridToPixelSize(int rows, int cols) {
    int requested_width = static_cast<int>(ceilf(_dwrite->_font_width) * cols);
    int requested_height =
        static_cast<int>(ceilf(_dwrite->_font_height) * rows);

    // Adjust size to include title bar
    RECT adjusted_rect = {0, 0, requested_width, requested_height};
    AdjustWindowRect(&adjusted_rect, WS_OVERLAPPEDWINDOW, false);
    return {.width = (UINT)(adjusted_rect.right - adjusted_rect.left),
            .height = (UINT)(adjusted_rect.bottom - adjusted_rect.top)};
  }

  D2D1_SIZE_U SetDpiScale(float current_dpi) {
    _dwrite->SetDpiScale(current_dpi);
    return FontSize();
  }

  D2D1_SIZE_U ResizeFont(float size) {
    _dwrite->ResizeFont(size);
    return FontSize();
  }

  void OnRowsCols(const on_rows_cols_t &callback) { _on_rows_cols = callback; }

  HRESULT
  DrawGlyphRun(float baseline_origin_x, float baseline_origin_y,
               DWRITE_MEASURING_MODE measuring_mode,
               DWRITE_GLYPH_RUN const *glyph_run,
               DWRITE_GLYPH_RUN_DESCRIPTION const *glyph_run_description,
               IUnknown *client_drawing_effect) {
    HRESULT hr = S_OK;
    if (client_drawing_effect) {
      ComPtr<GlyphDrawingEffect> drawing_effect;
      client_drawing_effect->QueryInterface(
          __uuidof(GlyphDrawingEffect),
          reinterpret_cast<void **>(drawing_effect.ReleaseAndGetAddressOf()));
      _device->_drawing_effect_brush->SetColor(
          D2D1::ColorF(drawing_effect->_text_color));
    } else {
      _device->_drawing_effect_brush->SetColor(
          D2D1::ColorF(_defaultHL->foreground));
    }

    DWRITE_GLYPH_IMAGE_FORMATS supported_formats =
        DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_CFF |
        DWRITE_GLYPH_IMAGE_FORMATS_COLR | DWRITE_GLYPH_IMAGE_FORMATS_SVG |
        DWRITE_GLYPH_IMAGE_FORMATS_PNG | DWRITE_GLYPH_IMAGE_FORMATS_JPEG |
        DWRITE_GLYPH_IMAGE_FORMATS_TIFF |
        DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8;

    ComPtr<IDWriteColorGlyphRunEnumerator1> glyph_run_enumerator;
    hr = _dwrite->_dwrite_factory->TranslateColorGlyphRun(
        D2D1_POINT_2F{.x = baseline_origin_x, .y = baseline_origin_y},
        glyph_run, glyph_run_description, supported_formats, measuring_mode,
        nullptr, 0, &glyph_run_enumerator);

    if (hr == DWRITE_E_NOCOLOR) {
      _device->_d2d_context->DrawGlyphRun(
          D2D1_POINT_2F{.x = baseline_origin_x, .y = baseline_origin_y},
          glyph_run, _device->_drawing_effect_brush.Get(), measuring_mode);
    } else {
      assert(!FAILED(hr));

      while (true) {
        BOOL has_run;
        WIN_CHECK(glyph_run_enumerator->MoveNext(&has_run));
        if (!has_run) {
          break;
        }

        DWRITE_COLOR_GLYPH_RUN1 const *color_run;
        WIN_CHECK(glyph_run_enumerator->GetCurrentRun(&color_run));

        D2D1_POINT_2F current_baseline_origin{.x = color_run->baselineOriginX,
                                              .y = color_run->baselineOriginY};

        switch (color_run->glyphImageFormat) {
        case DWRITE_GLYPH_IMAGE_FORMATS_PNG:
        case DWRITE_GLYPH_IMAGE_FORMATS_JPEG:
        case DWRITE_GLYPH_IMAGE_FORMATS_TIFF:
        case DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8: {
          _device->_d2d_context->DrawColorBitmapGlyphRun(
              color_run->glyphImageFormat, current_baseline_origin,
              &color_run->glyphRun, measuring_mode);
        } break;
        case DWRITE_GLYPH_IMAGE_FORMATS_SVG: {
          _device->_d2d_context->DrawSvgGlyphRun(
              current_baseline_origin, &color_run->glyphRun,
              _device->_drawing_effect_brush.Get(), nullptr, 0, measuring_mode);
        } break;
        case DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE:
        case DWRITE_GLYPH_IMAGE_FORMATS_CFF:
        case DWRITE_GLYPH_IMAGE_FORMATS_COLR:
        default: {
          bool use_palette_color = color_run->paletteIndex != 0xFFFF;
          if (use_palette_color) {
            _device->_temp_brush->SetColor(color_run->runColor);
          }

          _device->_d2d_context->PushAxisAlignedClip(
              D2D1_RECT_F{
                  .left = current_baseline_origin.x,
                  .top = current_baseline_origin.y - _dwrite->_font_ascent,
                  .right = current_baseline_origin.x +
                           (color_run->glyphRun.glyphCount * 2 *
                            _dwrite->_font_width),
                  .bottom = current_baseline_origin.y + _dwrite->_font_descent,
              },
              D2D1_ANTIALIAS_MODE_ALIASED);
          _device->_d2d_context->DrawGlyphRun(
              current_baseline_origin, &color_run->glyphRun,
              color_run->glyphRunDescription,
              use_palette_color ? _device->_temp_brush.Get()
                                : _device->_drawing_effect_brush.Get(),
              measuring_mode);
          _device->_d2d_context->PopAxisAlignedClip();
        } break;
        }
      }
    }

    return hr;
  }

  HRESULT DrawUnderline(float baseline_origin_x, float baseline_origin_y,
                        DWRITE_UNDERLINE const *underline,
                        IUnknown *client_drawing_effect) {
    HRESULT hr = S_OK;
    if (client_drawing_effect) {
      ComPtr<GlyphDrawingEffect> drawing_effect;
      client_drawing_effect->QueryInterface(
          __uuidof(GlyphDrawingEffect),
          reinterpret_cast<void **>(drawing_effect.ReleaseAndGetAddressOf()));
      _device->_temp_brush->SetColor(
          D2D1::ColorF(drawing_effect->_special_color));
    } else {
      _device->_temp_brush->SetColor(D2D1::ColorF(_defaultHL->special));
    }

    D2D1_RECT_F rect =
        D2D1_RECT_F{.left = baseline_origin_x,
                    .top = baseline_origin_y + underline->offset,
                    .right = baseline_origin_x + underline->width,
                    .bottom = baseline_origin_y + underline->offset +
                              std::max(underline->thickness, 1.0f)};

    _device->_d2d_context->FillRectangle(rect, _device->_temp_brush.Get());
    return hr;
  }

  HRESULT GetCurrentTransform(DWRITE_MATRIX *transform) {
    _device->_d2d_context->GetTransform(
        reinterpret_cast<D2D1_MATRIX_3X2_F *>(transform));
    return S_OK;
  }
};

HRESULT
GlyphRenderer::DrawGlyphRun(
    void *client_drawing_context, float baseline_origin_x,
    float baseline_origin_y, DWRITE_MEASURING_MODE measuring_mode,
    DWRITE_GLYPH_RUN const *glyph_run,
    DWRITE_GLYPH_RUN_DESCRIPTION const *glyph_run_description,
    IUnknown *client_drawing_effect) noexcept {
  auto renderer = reinterpret_cast<RendererImpl *>(client_drawing_context);
  return renderer->DrawGlyphRun(baseline_origin_x, baseline_origin_y,
                                measuring_mode, glyph_run,
                                glyph_run_description, client_drawing_effect);
}

HRESULT GlyphRenderer::DrawUnderline(void *client_drawing_context,
                                     float baseline_origin_x,
                                     float baseline_origin_y,
                                     DWRITE_UNDERLINE const *underline,
                                     IUnknown *client_drawing_effect) noexcept {
  auto renderer = reinterpret_cast<RendererImpl *>(client_drawing_context);
  return renderer->DrawUnderline(baseline_origin_x, baseline_origin_y,
                                 underline, client_drawing_effect);
}

HRESULT GlyphRenderer::GetCurrentTransform(void *client_drawing_context,
                                           DWRITE_MATRIX *transform) noexcept {
  auto renderer = reinterpret_cast<RendererImpl *>(client_drawing_context);
  return renderer->GetCurrentTransform(transform);
}

///
/// Renderer
///
Renderer::Renderer(HWND hwnd, bool disable_ligatures, float linespace_factor,
                   const HighlightAttribute *defaultHL)
    : _impl(new RendererImpl(hwnd, disable_ligatures, linespace_factor,
                             defaultHL)) {}

Renderer::~Renderer() { delete _impl; }

D2D1_SIZE_U Renderer::FontSize() const { return _impl->FontSize(); }

void Renderer::Resize(uint32_t width, uint32_t height) {
  _impl->Resize(width, height);
}

D2D1_SIZE_U Renderer::Size() const { return _impl->Size(); }

void Renderer::DrawGridLine(const NvimGrid *grid, int row) {
  _impl->DrawGridLine(grid, row);
}

void Renderer::DrawCursor(const NvimGrid *grid) { _impl->DrawCursor(grid); }

void Renderer::DrawBorderRectangles(const NvimGrid *grid) {
  _impl->DrawBorderRectangles(grid);
}

void Renderer::UpdateGuiFont(const char *guifont, size_t strlen) {
  if (strlen == 0) {
    return;
  }

  const char *size_str = strstr(guifont, ":h");
  if (!size_str) {
    return;
  }

  size_t font_str_len = size_str - guifont;
  size_t size_str_len = strlen - (font_str_len + 2);
  size_str += 2;

  float font_size = DEFAULT_FONT_SIZE;
  // Assume font size part of string is less than 256 characters
  if (size_str_len < 256) {
    char font_size_str[256];
    memcpy(font_size_str, size_str, size_str_len);
    font_size_str[size_str_len] = '\0';
    font_size = static_cast<float>(atof(font_size_str));
  }

  _impl->UpdateFont(font_size, guifont, static_cast<int>(font_str_len));
}

void Renderer::DrawBackgroundRect(int rows, int cols,
                                  const HighlightAttribute *hl) {
  _impl->DrawBackgroundRect(rows, cols, hl);
}

void Renderer::StartDraw() { _impl->StartDraw(); }

void Renderer::FinishDraw() { _impl->FinishDraw(); }

D2D1_SIZE_U Renderer::GridToPixelSize(int rows, int cols) {
  return _impl->GridToPixelSize(rows, cols);
}

void Renderer::OnRowsCols(const on_rows_cols_t &callback) {
  _impl->OnRowsCols(callback);
}
