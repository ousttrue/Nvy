#include "renderer.h"
#include <algorithm>
#include <assert.h>
#include <d2d1_3.h>
#include <d3d11_4.h>
#include <dwrite_3.h>
#include <dxgi1_2.h>
#include <nvim_grid.h>
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

  void UpdateFont(float font_size, std::string_view font_string = {}) {
    this->_dwrite_text_format.Reset();
    this->UpdateFontMetrics(font_size, font_string);
  }

  void UpdateFontMetrics(float font_size, std::string_view font_string) {
    if (font_size == 0) {
      return;
    }

    font_size = std::max(5.0f, std::min(font_size, 150.0f));
    this->_last_requested_font_size = font_size;

    ComPtr<IDWriteFontCollection> font_collection;
    auto hr = this->_dwrite_factory->GetSystemFontCollection(&font_collection);
    if (FAILED(hr)) {
      return;
    }

    int wstrlen = MultiByteToWideChar(CP_UTF8, 0, font_string.data(),
                                      font_string.size(), 0, 0);
    if (wstrlen != 0 && wstrlen < MAX_FONT_LENGTH) {
      MultiByteToWideChar(CP_UTF8, 0, font_string.data(), font_string.size(),
                          this->_font, MAX_FONT_LENGTH - 1);
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
    hr = font_collection->GetFontFamily(index, &font_family);
    if (FAILED(hr)) {
      return;
    }

    ComPtr<IDWriteFont> write_font;
    hr = font_family->GetFirstMatchingFont(
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, &write_font);
    if (FAILED(hr)) {
      return;
    }

    ComPtr<IDWriteFontFace> font_face;
    hr = write_font->CreateFontFace(&font_face);
    if (FAILED(hr)) {
      return;
    }

    hr = font_face->QueryInterface<IDWriteFontFace1>(&this->_font_face);
    if (FAILED(hr)) {
      return;
    }

    this->_font_face->GetMetrics(&this->_font_metrics);

    uint16_t glyph_index;
    constexpr uint32_t codepoint = L'A';
    hr = this->_font_face->GetGlyphIndicesW(&codepoint, 1, &glyph_index);
    if (FAILED(hr)) {
      return;
    }

    int32_t glyph_advance_in_em;
    hr = this->_font_face->GetDesignGlyphAdvances(1, &glyph_index,
                                                  &glyph_advance_in_em);
    if (FAILED(hr)) {
      return;
    }

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

    hr = this->_dwrite_factory->CreateTextFormat(
        this->_font, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, this->_font_size,
        L"en-us", &this->_dwrite_text_format);
    if (FAILED(hr)) {
      return;
    }

    hr = this->_dwrite_text_format->SetLineSpacing(
        DWRITE_LINE_SPACING_METHOD_UNIFORM, this->_font_height,
        this->_font_ascent * this->_linespace_factor);
    if (FAILED(hr)) {
      return;
    }

    hr = this->_dwrite_text_format->SetParagraphAlignment(
        DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    if (FAILED(hr)) {
      return;
    }

    hr = this->_dwrite_text_format->SetWordWrapping(
        DWRITE_WORD_WRAPPING_NO_WRAP);
    if (FAILED(hr)) {
      return;
    }
  }

  float GetTextWidth(const wchar_t *text, uint32_t length) {
    // Create dummy text format to hit test the width of the font
    ComPtr<IDWriteTextLayout> test_text_layout;
    auto hr = this->_dwrite_factory->CreateTextLayout(
        text, length, this->_dwrite_text_format.Get(), 0.0f, 0.0f,
        &test_text_layout);
    if (FAILED(hr)) {
      return {};
    }

    DWRITE_HIT_TEST_METRICS metrics;
    float _;
    hr = test_text_layout->HitTestTextPosition(0, 0, &_, &_, &metrics);
    if (FAILED(hr)) {
      return {};
    }

    return metrics.width;
  }

  ComPtr<IDWriteTextLayout1>
  GetTextLayout(const D2D1_RECT_F &rect, const wchar_t *text, uint32_t length) {
    ComPtr<IDWriteTextLayout> temp_text_layout;
    auto hr = this->_dwrite_factory->CreateTextLayout(
        text, length, this->_dwrite_text_format.Get(), rect.right - rect.left,
        rect.bottom - rect.top, &temp_text_layout);
    if (FAILED(hr)) {
      return {};
    }

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
  ComPtr<ID2D1Factory5> _d2d_factory;
  ComPtr<ID2D1Device4> _d2d_device;
  ComPtr<ID2D1DeviceContext4> _d2d_context;
  ComPtr<ID2D1SolidColorBrush> _d2d_background_rect_brush;
  ComPtr<ID2D1SolidColorBrush> _drawing_effect_brush;
  ComPtr<ID2D1SolidColorBrush> _temp_brush;

  ComPtr<GlyphRenderer> _glyph_renderer;

public:
  static std::unique_ptr<DeviceImpl>
  Create(const ComPtr<ID3D11Device2> &d3d_device) {

    auto p = std::unique_ptr<DeviceImpl>(new DeviceImpl);

    D2D1_FACTORY_OPTIONS options{};
#ifndef NDEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    auto hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options,
                                p->_d2d_factory.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
      return {};
    }

    ComPtr<IDXGIDevice3> dxgi_device;
    hr = d3d_device.As(&dxgi_device);
    if (FAILED(hr)) {
      return {};
    }

    hr = p->_d2d_factory->CreateDevice(dxgi_device.Get(), &p->_d2d_device);
    if (FAILED(hr)) {
      return {};
    }

    hr = p->_d2d_device->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS,
        &p->_d2d_context);
    if (FAILED(hr)) {
      return {};
    }

    hr = p->_d2d_context->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &p->_d2d_background_rect_brush);
    if (FAILED(hr)) {
      return {};
    }

    hr = p->_d2d_context->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &p->_drawing_effect_brush);
    if (FAILED(hr)) {
      return {};
    }

    hr = p->_d2d_context->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &p->_temp_brush);
    if (FAILED(hr)) {
      return {};
    }

    hr = GlyphRenderer::Create(&p->_glyph_renderer);
    if (FAILED(hr)) {
      assert(false);
      return {};
    }

    return p;
  }
};

class RendererImpl {
  ComPtr<ID3D11Device2> _d3d_device;
  ComPtr<IDXGISurface2> _dxgi_backbuffer;
  std::unique_ptr<class DeviceImpl> _device;
  std::unique_ptr<class DWriteImpl> _dwrite;

  bool _draw_active = false;

  const HighlightAttribute *_defaultHL = nullptr;

public:
  RendererImpl(const ComPtr<ID3D11Device2> &d3d_device, bool disable_ligatures,
               float linespace_factor, uint32_t monitor_dpi,
               const HighlightAttribute *defaultHL)
      : _d3d_device(d3d_device),
        _dwrite(DWriteImpl::Create(disable_ligatures, linespace_factor,
                                   monitor_dpi)),
        _defaultHL(defaultHL) {
    this->SetFont(DEFAULT_FONT, DEFAULT_FONT_SIZE);
  }

  void SetTarget(const ComPtr<IDXGISurface2> &backbuffer) {
    _dxgi_backbuffer = backbuffer;
  }

  std::tuple<float, float> FontSize() const {
    return {
        _dwrite->_font_width,
        _dwrite->_font_height,
    };
  }

  void SetFont(std::string_view font_string, float font_size) {
    _dwrite->UpdateFont(font_size, font_string);
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

  void DrawBorderRectangles(const NvimGrid *grid, int width, int height) {

    // auto size = _d2d_target_bitmap->GetPixelSize();
    // auto width = size.width;
    // auto height = size.height;

    float left_border = _dwrite->_font_width * grid->Cols();
    float top_border = _dwrite->_font_height * grid->Rows();

    if (left_border != static_cast<float>(width)) {
      D2D1_RECT_F vertical_rect{.left = left_border,
                                .top = 0.0f,
                                .right = static_cast<float>(width),
                                .bottom = static_cast<float>(height)};
      this->DrawBackgroundRect(vertical_rect, &grid->hl(0));
    }

    if (top_border != static_cast<float>(height)) {
      D2D1_RECT_F horizontal_rect{.left = 0.0f,
                                  .top = top_border,
                                  .right = static_cast<float>(width),
                                  .bottom = static_cast<float>(height)};
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

  std::tuple<int, int> StartDraw() {
    if (!_device) {
      _device = DeviceImpl::Create(_d3d_device.Get());
    }

    constexpr D2D1_BITMAP_PROPERTIES1 target_bitmap_properties{
        .pixelFormat = D2D1_PIXEL_FORMAT{.format = DXGI_FORMAT_B8G8R8A8_UNORM,
                                         .alphaMode = D2D1_ALPHA_MODE_IGNORE},
        .dpiX = DEFAULT_DPI,
        .dpiY = DEFAULT_DPI,
        .bitmapOptions =
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW};
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_target_bitmap;
    auto hr = _device->_d2d_context->CreateBitmapFromDxgiSurface(
        _dxgi_backbuffer.Get(), &target_bitmap_properties, &d2d_target_bitmap);
    if (FAILED(hr)) {
      return {};
    }

    _device->_d2d_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

    if (!this->_draw_active) {
      // _swapchain->Wait();

      _device->_d2d_context->SetTarget(d2d_target_bitmap.Get());
      _device->_d2d_context->BeginDraw();
      _device->_d2d_context->SetTransform(D2D1::IdentityMatrix());
      this->_draw_active = true;
    }

    auto size = d2d_target_bitmap->GetPixelSize();

    return {size.width, size.height};
  }

  void FinishDraw() {
    _device->_d2d_context->EndDraw();
    _device->_d2d_context->SetTarget(nullptr);
    this->_draw_active = false;
  }

  void SetDpiScale(float current_dpi) { _dwrite->SetDpiScale(current_dpi); }

  void ResizeFont(float size) { _dwrite->ResizeFont(size); }

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
        hr = glyph_run_enumerator->MoveNext(&has_run);
        if (FAILED(hr)) {
          assert(false);
          break;
        }
        if (!has_run) {
          break;
        }

        DWRITE_COLOR_GLYPH_RUN1 const *color_run;
        hr = glyph_run_enumerator->GetCurrentRun(&color_run);
        if (FAILED(hr)) {
          assert(false);
          break;
        }

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
Renderer::Renderer(ID3D11Device2 *device, bool disable_ligatures,
                   float linespace_factor, uint32_t monitor_dpi,
                   const HighlightAttribute *defaultHL)
    : _impl(new RendererImpl(device, disable_ligatures, linespace_factor,
                             monitor_dpi, defaultHL)) {}

Renderer::~Renderer() { delete _impl; }

void Renderer::SetTarget(IDXGISurface2 *backbuffer) {
  _impl->SetTarget(backbuffer);
}

std::tuple<float, float> Renderer::FontSize() const {
  return _impl->FontSize();
}

void Renderer::DrawGridLine(const NvimGrid *grid, int row) {
  _impl->DrawGridLine(grid, row);
}

void Renderer::DrawCursor(const NvimGrid *grid) { _impl->DrawCursor(grid); }

void Renderer::DrawBorderRectangles(const NvimGrid *grid, int width,
                                    int height) {
  _impl->DrawBorderRectangles(grid, width, height);
}

void Renderer::SetFont(std::string_view font, float size) {
  _impl->SetFont(font, size);
}

void Renderer::DrawBackgroundRect(int rows, int cols,
                                  const HighlightAttribute *hl) {
  _impl->DrawBackgroundRect(rows, cols, hl);
}

std::tuple<int, int> Renderer::StartDraw() { return _impl->StartDraw(); }

void Renderer::FinishDraw() { _impl->FinishDraw(); }
