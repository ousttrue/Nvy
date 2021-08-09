#include "renderer.h"
using namespace Microsoft::WRL;

struct DECLSPEC_UUID("8d4d2884-e4d9-11ea-87d0-0242ac130003") GlyphDrawingEffect
    : public IUnknown
{
    ULONG ref_count;
    uint32_t text_color;
    uint32_t special_color;

private:
    GlyphDrawingEffect(uint32_t text_color, uint32_t special_color)
        : ref_count(1), text_color(text_color), special_color(special_color)
    {
    }

public:
    inline ULONG AddRef() noexcept override
    {
        return InterlockedIncrement(&ref_count);
    }

    inline ULONG Release() noexcept override
    {
        ULONG new_count = InterlockedDecrement(&ref_count);
        if (new_count == 0)
        {
            delete this;
            return 0;
        }
        return new_count;
    }

    HRESULT QueryInterface(REFIID riid, void **ppv_object) noexcept override
    {
        if (__uuidof(GlyphDrawingEffect) == riid)
        {
            *ppv_object = this;
        }
        else if (__uuidof(IUnknown) == riid)
        {
            *ppv_object = this;
        }
        else
        {
            *ppv_object = nullptr;
            return E_FAIL;
        }

        this->AddRef();
        return S_OK;
    }

    static HRESULT Create(uint32_t text_color, uint32_t special_color,
                          GlyphDrawingEffect **pp)
    {
        auto p = new GlyphDrawingEffect(text_color, special_color);
        *pp = p;
        return S_OK;
    }
};

struct GlyphRenderer : public IDWriteTextRenderer
{
    ULONG ref_count;

private:
    GlyphRenderer() : ref_count(1)
    {
    }

public:
    static HRESULT Create(GlyphRenderer **pp)
    {
        auto p = new GlyphRenderer();
        *pp = p;
        return S_OK;
    }

    HRESULT
    DrawGlyphRun(void *client_drawing_context, float baseline_origin_x,
                 float baseline_origin_y, DWRITE_MEASURING_MODE measuring_mode,
                 DWRITE_GLYPH_RUN const *glyph_run,
                 DWRITE_GLYPH_RUN_DESCRIPTION const *glyph_run_description,
                 IUnknown *client_drawing_effect) noexcept override
    {
        auto renderer = reinterpret_cast<Renderer *>(client_drawing_context);
        return renderer->DrawGlyphRun(
            baseline_origin_x, baseline_origin_y, measuring_mode, glyph_run,
            glyph_run_description, client_drawing_effect);
    }

    HRESULT DrawInlineObject(void *client_drawing_context, float origin_x,
                             float origin_y, IDWriteInlineObject *inline_obj,
                             BOOL is_sideways, BOOL is_right_to_left,
                             IUnknown *client_drawing_effect) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT
    DrawStrikethrough(void *client_drawing_context, float baseline_origin_x,
                      float baseline_origin_y,
                      DWRITE_STRIKETHROUGH const *strikethrough,
                      IUnknown *client_drawing_effect) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT DrawUnderline(void *client_drawing_context, float baseline_origin_x,
                          float baseline_origin_y,
                          DWRITE_UNDERLINE const *underline,
                          IUnknown *client_drawing_effect) noexcept override
    {
        auto renderer = reinterpret_cast<Renderer *>(client_drawing_context);
        return renderer->DrawUnderline(baseline_origin_x, baseline_origin_y,
                                       underline, client_drawing_effect);
    }

    HRESULT IsPixelSnappingDisabled(void *client_drawing_context,
                                    BOOL *is_disabled) noexcept override
    {
        *is_disabled = false;
        return S_OK;
    }

    HRESULT GetCurrentTransform(void *client_drawing_context,
                                DWRITE_MATRIX *transform) noexcept override
    {
        Renderer *renderer =
            reinterpret_cast<Renderer *>(client_drawing_context);
        return renderer->GetCurrentTransform(transform);
    }

    HRESULT GetPixelsPerDip(void *client_drawing_context,
                            float *pixels_per_dip) noexcept override
    {
        *pixels_per_dip = 1.0f;
        return S_OK;
    }

    ULONG AddRef() noexcept override
    {
        return InterlockedIncrement(&ref_count);
    }

    ULONG Release() noexcept override
    {
        ULONG new_count = InterlockedDecrement(&ref_count);
        if (new_count == 0)
        {
            delete this;
            return 0;
        }
        return new_count;
    }

    HRESULT QueryInterface(REFIID riid, void **ppv_object) noexcept override
    {
        if (__uuidof(IDWriteTextRenderer) == riid)
        {
            *ppv_object = this;
        }
        else if (__uuidof(IDWritePixelSnapping) == riid)
        {
            *ppv_object = this;
        }
        else if (__uuidof(IUnknown) == riid)
        {
            *ppv_object = this;
        }
        else
        {
            *ppv_object = nullptr;
            return E_FAIL;
        }

        this->AddRef();
        return S_OK;
    }
};

void Renderer::InitializeD2D()
{
    D2D1_FACTORY_OPTIONS options{};
#ifndef NDEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    WIN_CHECK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options,
                                &this->d2d_factory));
}

void Renderer::InitializeD3D()
{
    uint32_t flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Force DirectX 11.1
    ID3D11Device *temp_device;
    ID3D11DeviceContext *temp_context;
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1};
    WIN_CHECK(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, feature_levels,
        ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &temp_device,
        &this->d3d_feature_level, &temp_context));
    WIN_CHECK(temp_device->QueryInterface(
        __uuidof(ID3D11Device2), reinterpret_cast<void **>(&this->d3d_device)));
    WIN_CHECK(temp_context->QueryInterface(
        __uuidof(ID3D11DeviceContext2),
        reinterpret_cast<void **>(&this->d3d_context)));

    IDXGIDevice3 *dxgi_device;
    WIN_CHECK(this->d3d_device->QueryInterface(
        __uuidof(IDXGIDevice3), reinterpret_cast<void **>(&dxgi_device)));
    WIN_CHECK(this->d2d_factory->CreateDevice(dxgi_device, &this->d2d_device));
    WIN_CHECK(this->d2d_device->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS,
        &this->d2d_context));
    WIN_CHECK(this->d2d_context->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &this->d2d_background_rect_brush));

    WIN_CHECK(d2d_context->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &drawing_effect_brush));
    WIN_CHECK(d2d_context->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Black), &temp_brush));

    SafeRelease(&dxgi_device);
}

void Renderer::InitializeDWrite()
{
    WIN_CHECK(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory4),
        reinterpret_cast<IUnknown **>(&this->dwrite_factory)));
    if (this->disable_ligatures)
    {
        WIN_CHECK(
            this->dwrite_factory->CreateTypography(&this->dwrite_typography));
        WIN_CHECK(this->dwrite_typography->AddFontFeature(DWRITE_FONT_FEATURE{
            .nameTag = DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES,
            .parameter = 0}));
    }
}

void Renderer::InitializeWindowDependentResources()
{
    if (this->dxgi_swapchain)
    {
        this->d2d_target_bitmap->Release();

        HRESULT hr = this->dxgi_swapchain->ResizeBuffers(
            2, this->pixel_size.width, this->pixel_size.height,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);

        if (hr == DXGI_ERROR_DEVICE_REMOVED)
        {
            this->HandleDeviceLost();
        }
    }
    else
    {
        DXGI_SWAP_CHAIN_DESC1 swapchain_desc{
            .Width = this->pixel_size.width,
            .Height = this->pixel_size.height,
            .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
            .SampleDesc = {.Count = 1, .Quality = 0},
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = 2,
            .Scaling = DXGI_SCALING_NONE,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
            .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
            .Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
                     DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING};

        IDXGIDevice3 *dxgi_device;
        WIN_CHECK(this->d3d_device->QueryInterface(
            __uuidof(IDXGIDevice3), reinterpret_cast<void **>(&dxgi_device)));
        IDXGIAdapter *dxgi_adapter;
        WIN_CHECK(dxgi_device->GetAdapter(&dxgi_adapter));
        IDXGIFactory2 *dxgi_factory;
        WIN_CHECK(dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)));

        IDXGISwapChain1 *dxgi_swapchain_temp;
        WIN_CHECK(dxgi_factory->CreateSwapChainForHwnd(
            this->d3d_device, this->hwnd, &swapchain_desc, nullptr, nullptr,
            &dxgi_swapchain_temp));
        WIN_CHECK(dxgi_factory->MakeWindowAssociation(this->hwnd,
                                                      DXGI_MWA_NO_ALT_ENTER));
        WIN_CHECK(dxgi_swapchain_temp->QueryInterface(
            __uuidof(IDXGISwapChain2),
            reinterpret_cast<void **>(&this->dxgi_swapchain)));

        WIN_CHECK(this->dxgi_swapchain->SetMaximumFrameLatency(1));
        this->swapchain_wait_handle =
            this->dxgi_swapchain->GetFrameLatencyWaitableObject();

        SafeRelease(&dxgi_swapchain_temp);
        SafeRelease(&dxgi_device);
        SafeRelease(&dxgi_adapter);
        SafeRelease(&dxgi_factory);
    }

    constexpr D2D1_BITMAP_PROPERTIES1 target_bitmap_properties{
        .pixelFormat = D2D1_PIXEL_FORMAT{.format = DXGI_FORMAT_B8G8R8A8_UNORM,
                                         .alphaMode = D2D1_ALPHA_MODE_IGNORE},
        .dpiX = DEFAULT_DPI,
        .dpiY = DEFAULT_DPI,
        .bitmapOptions =
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW};
    IDXGISurface2 *dxgi_backbuffer;
    WIN_CHECK(
        this->dxgi_swapchain->GetBuffer(0, IID_PPV_ARGS(&dxgi_backbuffer)));
    WIN_CHECK(this->d2d_context->CreateBitmapFromDxgiSurface(
        dxgi_backbuffer, &target_bitmap_properties, &this->d2d_target_bitmap));
    this->d2d_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

    SafeRelease(&dxgi_backbuffer);
}

void Renderer::HandleDeviceLost()
{
    SafeRelease(&this->d3d_device);
    SafeRelease(&this->d3d_context);
    SafeRelease(&this->dxgi_swapchain);
    SafeRelease(&this->d2d_factory);
    SafeRelease(&this->d2d_device);
    SafeRelease(&this->d2d_context);
    SafeRelease(&this->d2d_target_bitmap);
    SafeRelease(&this->d2d_background_rect_brush);
    SafeRelease(&this->dwrite_factory);
    SafeRelease(&this->dwrite_text_format);
    if (this->glyph_renderer)
    {
        delete this->glyph_renderer;
        this->glyph_renderer = nullptr;
    }

    this->InitializeD2D();
    this->InitializeD3D();
    this->InitializeDWrite();
    auto hr = GlyphRenderer::Create(&this->glyph_renderer);
    assert(hr == S_OK);
    this->UpdateFont(DEFAULT_FONT_SIZE, DEFAULT_FONT,
                     static_cast<int>(strlen(DEFAULT_FONT)));
}

Renderer::Renderer(HWND hwnd, bool disable_ligatures, float linespace_factor,
                   float monitor_dpi)
{
    this->hwnd = hwnd;
    this->disable_ligatures = disable_ligatures;
    this->linespace_factor = linespace_factor;

    this->dpi_scale = monitor_dpi / 96.0f;
    this->hl_attribs.resize(MAX_HIGHLIGHT_ATTRIBS);

    this->HandleDeviceLost();
}

void Renderer::Attach()
{
    RECT client_rect;
    GetClientRect(this->hwnd, &client_rect);
    pixel_size.width =
        static_cast<uint32_t>(client_rect.right - client_rect.left);
    pixel_size.height =
        static_cast<uint32_t>(client_rect.bottom - client_rect.top);
}

Renderer::~Renderer()
{
    SafeRelease(&this->d3d_device);
    SafeRelease(&this->d3d_context);
    SafeRelease(&this->dxgi_swapchain);
    SafeRelease(&this->d2d_factory);
    SafeRelease(&this->d2d_device);
    SafeRelease(&this->d2d_context);
    SafeRelease(&this->d2d_target_bitmap);
    SafeRelease(&this->d2d_background_rect_brush);
    SafeRelease(&this->dwrite_factory);
    SafeRelease(&this->dwrite_text_format);
    delete this->glyph_renderer;

    free(this->grid_chars);
    free(this->grid_cell_properties);
}

void Renderer::Resize(uint32_t width, uint32_t height)
{
    pixel_size.width = width;
    pixel_size.height = height;
}

float Renderer::GetTextWidth(wchar_t *text, uint32_t length)
{
    // Create dummy text format to hit test the width of the font
    IDWriteTextLayout *test_text_layout = nullptr;
    WIN_CHECK(this->dwrite_factory->CreateTextLayout(
        text, length, this->dwrite_text_format, 0.0f, 0.0f, &test_text_layout));

    DWRITE_HIT_TEST_METRICS metrics;
    float _;
    WIN_CHECK(test_text_layout->HitTestTextPosition(0, 0, &_, &_, &metrics));
    test_text_layout->Release();

    return metrics.width;
}

void Renderer::UpdateFontMetrics(float font_size, const char *font_string,
                                 int strlen)
{
    font_size = max(5.0f, min(font_size, 150.0f));
    this->last_requested_font_size = font_size;

    IDWriteFontCollection *font_collection;
    WIN_CHECK(this->dwrite_factory->GetSystemFontCollection(&font_collection));

    int wstrlen = MultiByteToWideChar(CP_UTF8, 0, font_string, strlen, 0, 0);
    if (wstrlen != 0 && wstrlen < MAX_FONT_LENGTH)
    {
        MultiByteToWideChar(CP_UTF8, 0, font_string, strlen, this->font,
                            MAX_FONT_LENGTH - 1);
        this->font[wstrlen] = L'\0';
    }

    uint32_t index;
    BOOL exists;
    font_collection->FindFamilyName(this->font, &index, &exists);

    const wchar_t *fallback_font = L"Consolas";
    if (!exists)
    {
        font_collection->FindFamilyName(fallback_font, &index, &exists);
        memcpy(this->font, fallback_font,
               (wcslen(fallback_font) + 1) * sizeof(wchar_t));
    }

    IDWriteFontFamily *font_family;
    WIN_CHECK(font_collection->GetFontFamily(index, &font_family));

    IDWriteFont *write_font;
    WIN_CHECK(font_family->GetFirstMatchingFont(
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, &write_font));

    IDWriteFontFace *font_face;
    WIN_CHECK(write_font->CreateFontFace(&font_face));
    WIN_CHECK(font_face->QueryInterface<IDWriteFontFace1>(&this->font_face));

    this->font_face->GetMetrics(&this->font_metrics);

    uint16_t glyph_index;
    constexpr uint32_t codepoint = L'A';
    WIN_CHECK(this->font_face->GetGlyphIndicesW(&codepoint, 1, &glyph_index));

    int32_t glyph_advance_in_em;
    WIN_CHECK(this->font_face->GetDesignGlyphAdvances(1, &glyph_index,
                                                      &glyph_advance_in_em));

    float desired_height =
        font_size * this->dpi_scale * (DEFAULT_DPI / POINTS_PER_INCH);
    float width_advance = static_cast<float>(glyph_advance_in_em) /
                          this->font_metrics.designUnitsPerEm;
    float desired_width = desired_height * width_advance;

    // We need the width to be aligned on a per-pixel boundary, thus we will
    // roundf the desired_width and calculate the font size given the new exact
    // width
    this->font_width = roundf(desired_width);
    this->font_size = this->font_width / width_advance;
    float frac_font_ascent = (this->font_size * this->font_metrics.ascent) /
                             this->font_metrics.designUnitsPerEm;
    float frac_font_descent = (this->font_size * this->font_metrics.descent) /
                              this->font_metrics.designUnitsPerEm;
    float linegap = (this->font_size * this->font_metrics.lineGap) /
                    this->font_metrics.designUnitsPerEm;
    float half_linegap = linegap / 2.0f;
    this->font_ascent = ceilf(frac_font_ascent + half_linegap);
    this->font_descent = ceilf(frac_font_descent + half_linegap);
    this->font_height = this->font_ascent + this->font_descent;
    this->font_height *= this->linespace_factor;

    WIN_CHECK(this->dwrite_factory->CreateTextFormat(
        this->font, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, this->font_size,
        L"en-us", &this->dwrite_text_format));

    WIN_CHECK(this->dwrite_text_format->SetLineSpacing(
        DWRITE_LINE_SPACING_METHOD_UNIFORM, this->font_height,
        this->font_ascent * this->linespace_factor));
    WIN_CHECK(this->dwrite_text_format->SetParagraphAlignment(
        DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    WIN_CHECK(this->dwrite_text_format->SetWordWrapping(
        DWRITE_WORD_WRAPPING_NO_WRAP));

    SafeRelease(&font_face);
    SafeRelease(&write_font);
    SafeRelease(&font_family);
    SafeRelease(&font_collection);
}

void Renderer::UpdateFont(float font_size, const char *font_string, int strlen)
{
    if (this->dwrite_text_format)
    {
        this->dwrite_text_format->Release();
    }

    this->UpdateFontMetrics(font_size, font_string, strlen);
}

void Renderer::UpdateDefaultColors(mpack_node_t default_colors)
{
    size_t default_colors_arr_length = mpack_node_array_length(default_colors);

    for (size_t i = 1; i < default_colors_arr_length; ++i)
    {
        mpack_node_t color_arr = mpack_node_array_at(default_colors, i);

        // Default colors occupy the first index of the highlight attribs array
        this->hl_attribs[0].foreground = static_cast<uint32_t>(
            mpack_node_array_at(color_arr, 0).data->value.u);
        this->hl_attribs[0].background = static_cast<uint32_t>(
            mpack_node_array_at(color_arr, 1).data->value.u);
        this->hl_attribs[0].special = static_cast<uint32_t>(
            mpack_node_array_at(color_arr, 2).data->value.u);
        this->hl_attribs[0].flags = 0;
    }
}

void Renderer::UpdateHighlightAttributes(mpack_node_t highlight_attribs)
{
    uint64_t attrib_count = mpack_node_array_length(highlight_attribs);
    for (uint64_t i = 1; i < attrib_count; ++i)
    {
        int64_t attrib_index =
            mpack_node_array_at(mpack_node_array_at(highlight_attribs, i), 0)
                .data->value.i;
        assert(attrib_index <= MAX_HIGHLIGHT_ATTRIBS);

        mpack_node_t attrib_map =
            mpack_node_array_at(mpack_node_array_at(highlight_attribs, i), 1);

        const auto SetColor = [&](const char *name, uint32_t *color) {
            mpack_node_t color_node =
                mpack_node_map_cstr_optional(attrib_map, name);
            if (!mpack_node_is_missing(color_node))
            {
                *color = static_cast<uint32_t>(color_node.data->value.u);
            }
            else
            {
                *color = DEFAULT_COLOR;
            }
        };
        SetColor("foreground", &this->hl_attribs[attrib_index].foreground);
        SetColor("background", &this->hl_attribs[attrib_index].background);
        SetColor("special", &this->hl_attribs[attrib_index].special);

        const auto SetFlag = [&](const char *flag_name,
                                 HighlightAttributeFlags flag) {
            mpack_node_t flag_node =
                mpack_node_map_cstr_optional(attrib_map, flag_name);
            if (!mpack_node_is_missing(flag_node))
            {
                if (flag_node.data->value.b)
                {
                    this->hl_attribs[attrib_index].flags |= flag;
                }
                else
                {
                    this->hl_attribs[attrib_index].flags &= ~flag;
                }
            }
        };
        SetFlag("reverse", HL_ATTRIB_REVERSE);
        SetFlag("italic", HL_ATTRIB_ITALIC);
        SetFlag("bold", HL_ATTRIB_BOLD);
        SetFlag("strikethrough", HL_ATTRIB_STRIKETHROUGH);
        SetFlag("underline", HL_ATTRIB_UNDERLINE);
        SetFlag("undercurl", HL_ATTRIB_UNDERCURL);
    }
}

uint32_t Renderer::CreateForegroundColor(HighlightAttributes *hl_attribs)
{
    if (hl_attribs->flags & HL_ATTRIB_REVERSE)
    {
        return hl_attribs->background == DEFAULT_COLOR
                   ? this->hl_attribs[0].background
                   : hl_attribs->background;
    }
    else
    {
        return hl_attribs->foreground == DEFAULT_COLOR
                   ? this->hl_attribs[0].foreground
                   : hl_attribs->foreground;
    }
}

uint32_t Renderer::CreateBackgroundColor(HighlightAttributes *hl_attribs)
{
    if (hl_attribs->flags & HL_ATTRIB_REVERSE)
    {
        return hl_attribs->foreground == DEFAULT_COLOR
                   ? this->hl_attribs[0].foreground
                   : hl_attribs->foreground;
    }
    else
    {
        return hl_attribs->background == DEFAULT_COLOR
                   ? this->hl_attribs[0].background
                   : hl_attribs->background;
    }
}

uint32_t Renderer::CreateSpecialColor(HighlightAttributes *hl_attribs)
{
    return hl_attribs->special == DEFAULT_COLOR ? this->hl_attribs[0].special
                                                : hl_attribs->special;
}

void Renderer::ApplyHighlightAttributes(HighlightAttributes *hl_attribs,
                                        IDWriteTextLayout *text_layout,
                                        int start, int end)
{
    ComPtr<GlyphDrawingEffect> drawing_effect;
    GlyphDrawingEffect::Create(this->CreateForegroundColor(hl_attribs),
                               this->CreateSpecialColor(hl_attribs),
                               &drawing_effect);
    DWRITE_TEXT_RANGE range{.startPosition = static_cast<uint32_t>(start),
                            .length = static_cast<uint32_t>(end - start)};
    if (hl_attribs->flags & HL_ATTRIB_ITALIC)
    {
        text_layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
    }
    if (hl_attribs->flags & HL_ATTRIB_BOLD)
    {
        text_layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
    }
    if (hl_attribs->flags & HL_ATTRIB_STRIKETHROUGH)
    {
        text_layout->SetStrikethrough(true, range);
    }
    if (hl_attribs->flags & HL_ATTRIB_UNDERLINE)
    {
        text_layout->SetUnderline(true, range);
    }
    if (hl_attribs->flags & HL_ATTRIB_UNDERCURL)
    {
        text_layout->SetUnderline(true, range);
    }
    text_layout->SetDrawingEffect(drawing_effect.Get(), range);
}

void Renderer::DrawBackgroundRect(D2D1_RECT_F rect,
                                  HighlightAttributes *hl_attribs)
{
    uint32_t color = this->CreateBackgroundColor(hl_attribs);
    this->d2d_background_rect_brush->SetColor(D2D1::ColorF(color));

    this->d2d_context->FillRectangle(rect, this->d2d_background_rect_brush);
}

D2D1_RECT_F Renderer::GetCursorForegroundRect(D2D1_RECT_F cursor_bg_rect)
{
    if (this->cursor.mode_info)
    {
        switch (this->cursor.mode_info->shape)
        {
        case CursorShape::None:
        {
        }
            return cursor_bg_rect;
        case CursorShape::Block:
        {
        }
            return cursor_bg_rect;
        case CursorShape::Vertical:
        {
            cursor_bg_rect.right = cursor_bg_rect.left + 2;
        }
            return cursor_bg_rect;
        case CursorShape::Horizontal:
        {
            cursor_bg_rect.top = cursor_bg_rect.bottom - 2;
        }
            return cursor_bg_rect;
        }
    }
    return cursor_bg_rect;
}

void Renderer::DrawHighlightedText(D2D1_RECT_F rect, wchar_t *text,
                                   uint32_t length,
                                   HighlightAttributes *hl_attribs)
{
    IDWriteTextLayout *text_layout = nullptr;
    WIN_CHECK(this->dwrite_factory->CreateTextLayout(
        text, length, this->dwrite_text_format, rect.right - rect.left,
        rect.bottom - rect.top, &text_layout));
    this->ApplyHighlightAttributes(hl_attribs, text_layout, 0, 1);

    this->d2d_context->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_ALIASED);
    text_layout->Draw(this, this->glyph_renderer, rect.left, rect.top);
    text_layout->Release();
    this->d2d_context->PopAxisAlignedClip();
}

void Renderer::DrawGridLine(int row)
{
    int base = row * this->grid_cols;

    D2D1_RECT_F rect{.left = 0.0f,
                     .top = row * this->font_height,
                     .right = this->grid_cols * this->font_width,
                     .bottom = (row * this->font_height) + this->font_height};

    IDWriteTextLayout *temp_text_layout = nullptr;
    WIN_CHECK(this->dwrite_factory->CreateTextLayout(
        &this->grid_chars[base], this->grid_cols, this->dwrite_text_format,
        rect.right - rect.left, rect.bottom - rect.top, &temp_text_layout));
    IDWriteTextLayout1 *text_layout;
    temp_text_layout->QueryInterface<IDWriteTextLayout1>(&text_layout);
    temp_text_layout->Release();

    uint16_t hl_attrib_id = this->grid_cell_properties[base].hl_attrib_id;
    int col_offset = 0;
    for (int i = 0; i < this->grid_cols; ++i)
    {
        // Add spacing for wide chars
        if (this->grid_cell_properties[base + i].is_wide_char)
        {
            float char_width =
                this->GetTextWidth(&this->grid_chars[base + i], 2);
            DWRITE_TEXT_RANGE range{.startPosition = static_cast<uint32_t>(i),
                                    .length = 1};
            text_layout->SetCharacterSpacing(
                0, (this->font_width * 2) - char_width, 0, range);
        }

        // Add spacing for unicode chars. These characters are still single char
        // width, but some of them by default will take up a bit more or less,
        // leading to issues. So we realign them here.
        else if (this->grid_chars[base + i] > 0xFF)
        {
            float char_width =
                this->GetTextWidth(&this->grid_chars[base + i], 1);
            if (abs(char_width - this->font_width) > 0.01f)
            {
                DWRITE_TEXT_RANGE range{
                    .startPosition = static_cast<uint32_t>(i), .length = 1};
                text_layout->SetCharacterSpacing(
                    0, this->font_width - char_width, 0, range);
            }
        }

        // Check if the attributes change,
        // if so draw until this point and continue with the new attributes
        if (this->grid_cell_properties[base + i].hl_attrib_id != hl_attrib_id)
        {
            D2D1_RECT_F bg_rect{.left = col_offset * this->font_width,
                                .top = row * this->font_height,
                                .right = col_offset * this->font_width +
                                         this->font_width * (i - col_offset),
                                .bottom = (row * this->font_height) +
                                          this->font_height};
            this->DrawBackgroundRect(bg_rect, &this->hl_attribs[hl_attrib_id]);
            this->ApplyHighlightAttributes(&this->hl_attribs[hl_attrib_id],
                                           text_layout, col_offset, i);

            hl_attrib_id = this->grid_cell_properties[base + i].hl_attrib_id;
            col_offset = i;
        }
    }

    // Draw the remaining columns, there is always atleast the last column to
    // draw, but potentially more in case the last X columns share the same
    // hl_attrib
    D2D1_RECT_F last_rect = rect;
    last_rect.left = col_offset * this->font_width;
    this->DrawBackgroundRect(last_rect, &this->hl_attribs[hl_attrib_id]);
    this->ApplyHighlightAttributes(&this->hl_attribs[hl_attrib_id], text_layout,
                                   col_offset, this->grid_cols);

    this->d2d_context->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_ALIASED);
    if (this->disable_ligatures)
    {
        text_layout->SetTypography(
            this->dwrite_typography,
            DWRITE_TEXT_RANGE{.startPosition = 0,
                              .length =
                                  static_cast<uint32_t>(this->grid_cols)});
    }
    text_layout->Draw(this, this->glyph_renderer, 0.0f, rect.top);
    this->d2d_context->PopAxisAlignedClip();
    text_layout->Release();
}

void Renderer::DrawGridLines(mpack_node_t grid_lines)
{
    assert(this->grid_chars != nullptr);
    assert(this->grid_cell_properties != nullptr);

    int grid_size = this->grid_cols * this->grid_rows;
    size_t line_count = mpack_node_array_length(grid_lines);
    for (size_t i = 1; i < line_count; ++i)
    {
        mpack_node_t grid_line = mpack_node_array_at(grid_lines, i);

        int row = MPackIntFromArray(grid_line, 1);
        int col_start = MPackIntFromArray(grid_line, 2);

        mpack_node_t cells_array = mpack_node_array_at(grid_line, 3);
        size_t cells_array_length = mpack_node_array_length(cells_array);

        int col_offset = col_start;
        int hl_attrib_id = 0;
        for (size_t j = 0; j < cells_array_length; ++j)
        {

            mpack_node_t cells = mpack_node_array_at(cells_array, j);
            size_t cells_length = mpack_node_array_length(cells);

            mpack_node_t text = mpack_node_array_at(cells, 0);
            const char *str = mpack_node_str(text);

            int strlen = static_cast<int>(mpack_node_strlen(text));

            if (cells_length > 1)
            {
                hl_attrib_id = MPackIntFromArray(cells, 1);
            }

            // Right part of double-width char is the empty string, thus
            // if the next cell array contains the empty string, we can process
            // the current string as a double-width char and proceed
            if (j < (cells_array_length - 1) &&
                mpack_node_strlen(mpack_node_array_at(
                    mpack_node_array_at(cells_array, j + 1), 0)) == 0)
            {

                int offset = row * this->grid_cols + col_offset;
                this->grid_cell_properties[offset].is_wide_char = true;
                this->grid_cell_properties[offset].hl_attrib_id = hl_attrib_id;
                this->grid_cell_properties[offset + 1].hl_attrib_id =
                    hl_attrib_id;

                int wstrlen = MultiByteToWideChar(CP_UTF8, 0, str, strlen,
                                                  &this->grid_chars[offset],
                                                  grid_size - offset);
                assert(wstrlen == 1 || wstrlen == 2);

                if (wstrlen == 1)
                {
                    this->grid_chars[offset + 1] = L'\0';
                }

                col_offset += 2;
                continue;
            }

            if (strlen == 0)
            {
                continue;
            }

            int repeat = 1;
            if (cells_length > 2)
            {
                repeat = MPackIntFromArray(cells, 2);
            }

            int offset = row * this->grid_cols + col_offset;
            int wstrlen = 0;
            for (int k = 0; k < repeat; ++k)
            {
                int idx = offset + (k * wstrlen);
                wstrlen = MultiByteToWideChar(CP_UTF8, 0, str, strlen,
                                              &this->grid_chars[idx],
                                              grid_size - idx);
            }

            int wstrlen_with_repetitions = wstrlen * repeat;
            for (int k = 0; k < wstrlen_with_repetitions; ++k)
            {
                this->grid_cell_properties[offset + k].hl_attrib_id =
                    hl_attrib_id;
                this->grid_cell_properties[offset + k].is_wide_char = false;
            }

            col_offset += wstrlen_with_repetitions;
        }

        this->DrawGridLine(row);
    }
}

void Renderer::DrawCursor()
{
    int cursor_grid_offset =
        this->cursor.row * this->grid_cols + this->cursor.col;

    int double_width_char_factor = 1;
    if (cursor_grid_offset < (this->grid_rows * this->grid_cols) &&
        this->grid_cell_properties[cursor_grid_offset].is_wide_char)
    {
        double_width_char_factor += 1;
    }

    HighlightAttributes cursor_hl_attribs =
        this->hl_attribs[this->cursor.mode_info->hl_attrib_id];
    if (this->cursor.mode_info->hl_attrib_id == 0)
    {
        cursor_hl_attribs.flags ^= HL_ATTRIB_REVERSE;
    }

    D2D1_RECT_F cursor_rect{
        .left = this->cursor.col * this->font_width,
        .top = this->cursor.row * this->font_height,
        .right = this->cursor.col * this->font_width +
                 this->font_width * double_width_char_factor,
        .bottom = (this->cursor.row * this->font_height) + this->font_height};
    D2D1_RECT_F cursor_fg_rect = this->GetCursorForegroundRect(cursor_rect);
    this->DrawBackgroundRect(cursor_fg_rect, &cursor_hl_attribs);

    if (this->cursor.mode_info->shape == CursorShape::Block)
    {
        this->DrawHighlightedText(cursor_fg_rect,
                                  &this->grid_chars[cursor_grid_offset],
                                  double_width_char_factor, &cursor_hl_attribs);
    }
}

void Renderer::UpdateGridSize(mpack_node_t grid_resize)
{
    mpack_node_t grid_resize_params = mpack_node_array_at(grid_resize, 1);
    int grid_cols = MPackIntFromArray(grid_resize_params, 1);
    int grid_rows = MPackIntFromArray(grid_resize_params, 2);

    if (this->grid_chars == nullptr || this->grid_cell_properties == nullptr ||
        this->grid_cols != grid_cols || this->grid_rows != grid_rows)
    {

        this->grid_cols = grid_cols;
        this->grid_rows = grid_rows;

        this->grid_chars = static_cast<wchar_t *>(malloc(
            static_cast<size_t>(grid_cols) * grid_rows * sizeof(wchar_t)));
        // Initialize all grid character to a space. An empty
        // grid cell is equivalent to a space in a text layout
        for (int i = 0; i < grid_cols * grid_rows; ++i)
        {
            this->grid_chars[i] = L' ';
        }
        this->grid_cell_properties = static_cast<CellProperty *>(calloc(
            static_cast<size_t>(grid_cols) * grid_rows, sizeof(CellProperty)));
    }
}

void Renderer::UpdateCursorPos(mpack_node_t cursor_goto)
{
    mpack_node_t cursor_goto_params = mpack_node_array_at(cursor_goto, 1);
    this->cursor.row = MPackIntFromArray(cursor_goto_params, 1);
    this->cursor.col = MPackIntFromArray(cursor_goto_params, 2);
}

void Renderer::UpdateCursorMode(mpack_node_t mode_change)
{
    mpack_node_t mode_change_params = mpack_node_array_at(mode_change, 1);
    this->cursor.mode_info =
        &this->cursor_mode_infos[mpack_node_array_at(mode_change_params, 1)
                                     .data->value.u];
}

void Renderer::UpdateCursorModeInfos(mpack_node_t mode_info_set_params)
{
    mpack_node_t mode_info_params =
        mpack_node_array_at(mode_info_set_params, 1);
    mpack_node_t mode_infos = mpack_node_array_at(mode_info_params, 1);
    size_t mode_infos_length = mpack_node_array_length(mode_infos);
    assert(mode_infos_length <= MAX_CURSOR_MODE_INFOS);

    for (size_t i = 0; i < mode_infos_length; ++i)
    {
        mpack_node_t mode_info_map = mpack_node_array_at(mode_infos, i);

        this->cursor_mode_infos[i].shape = CursorShape::None;
        mpack_node_t cursor_shape =
            mpack_node_map_cstr_optional(mode_info_map, "cursor_shape");
        if (!mpack_node_is_missing(cursor_shape))
        {
            const char *cursor_shape_str = mpack_node_str(cursor_shape);
            size_t strlen = mpack_node_strlen(cursor_shape);
            if (!strncmp(cursor_shape_str, "block", strlen))
            {
                this->cursor_mode_infos[i].shape = CursorShape::Block;
            }
            else if (!strncmp(cursor_shape_str, "vertical", strlen))
            {
                this->cursor_mode_infos[i].shape = CursorShape::Vertical;
            }
            else if (!strncmp(cursor_shape_str, "horizontal", strlen))
            {
                this->cursor_mode_infos[i].shape = CursorShape::Horizontal;
            }
        }

        this->cursor_mode_infos[i].hl_attrib_id = 0;
        mpack_node_t hl_attrib_index =
            mpack_node_map_cstr_optional(mode_info_map, "attr_id");
        if (!mpack_node_is_missing(hl_attrib_index))
        {
            this->cursor_mode_infos[i].hl_attrib_id =
                static_cast<int>(hl_attrib_index.data->value.i);
        }
    }
}

void Renderer::ScrollRegion(mpack_node_t scroll_region)
{
    mpack_node_t scroll_region_params = mpack_node_array_at(scroll_region, 1);

    int64_t top = mpack_node_array_at(scroll_region_params, 1).data->value.i;
    int64_t bottom = mpack_node_array_at(scroll_region_params, 2).data->value.i;
    int64_t left = mpack_node_array_at(scroll_region_params, 3).data->value.i;
    int64_t right = mpack_node_array_at(scroll_region_params, 4).data->value.i;
    int64_t rows = mpack_node_array_at(scroll_region_params, 5).data->value.i;
    int64_t cols = mpack_node_array_at(scroll_region_params, 6).data->value.i;

    // Currently nvim does not support horizontal scrolling,
    // the parameter is reserved for later use
    assert(cols == 0);

    // This part is slightly cryptic, basically we're just
    // iterating from top to bottom or vice versa depending on scroll direction.
    bool scrolling_down = rows > 0;
    int64_t start_row = scrolling_down ? top : bottom - 1;
    int64_t end_row = scrolling_down ? bottom - 1 : top;
    int64_t increment = scrolling_down ? 1 : -1;

    for (int64_t i = start_row; scrolling_down ? i <= end_row : i >= end_row;
         i += increment)
    {
        // Clip anything outside the scroll region
        int64_t target_row = i - rows;
        if (target_row < top || target_row >= bottom)
        {
            continue;
        }

        memcpy(&this->grid_chars[target_row * this->grid_cols + left],
               &this->grid_chars[i * this->grid_cols + left],
               (right - left) * sizeof(wchar_t));

        memcpy(&this->grid_cell_properties[target_row * this->grid_cols + left],
               &this->grid_cell_properties[i * this->grid_cols + left],
               (right - left) * sizeof(CellProperty));

        // Sadly I have given up on making use of IDXGISwapChain1::Present1
        // scroll_rects or bitmap copies. The former seems insufficient for
        // nvim since it can require multiple scrolls per frame, the latter
        // I can't seem to make work with the FLIP_SEQUENTIAL swapchain model.
        // Thus we fall back to drawing the appropriate scrolled grid lines
        this->DrawGridLine(target_row);
    }

    // Redraw the line which the cursor has moved to, as it is no
    // longer guaranteed that the cursor is still there
    int cursor_row = this->cursor.row - rows;
    if (cursor_row >= 0 && cursor_row < this->grid_rows)
    {
        this->DrawGridLine(cursor_row);
    }
}

void Renderer::DrawBorderRectangles()
{
    float left_border = this->font_width * this->grid_cols;
    float top_border = this->font_height * this->grid_rows;

    if (left_border != static_cast<float>(this->pixel_size.width))
    {
        D2D1_RECT_F vertical_rect{
            .left = left_border,
            .top = 0.0f,
            .right = static_cast<float>(this->pixel_size.width),
            .bottom = static_cast<float>(this->pixel_size.height)};
        this->DrawBackgroundRect(vertical_rect, &this->hl_attribs[0]);
    }

    if (top_border != static_cast<float>(this->pixel_size.height))
    {
        D2D1_RECT_F horizontal_rect{
            .left = 0.0f,
            .top = top_border,
            .right = static_cast<float>(this->pixel_size.width),
            .bottom = static_cast<float>(this->pixel_size.height)};
        this->DrawBackgroundRect(horizontal_rect, &this->hl_attribs[0]);
    }
}

void Renderer::UpdateGuiFont(const char *guifont, size_t strlen)
{
    if (strlen == 0)
    {
        return;
    }

    const char *size_str = strstr(guifont, ":h");
    if (!size_str)
    {
        return;
    }

    size_t font_str_len = size_str - guifont;
    size_t size_str_len = strlen - (font_str_len + 2);
    size_str += 2;

    float font_size = DEFAULT_FONT_SIZE;
    // Assume font size part of string is less than 256 characters
    if (size_str_len < 256)
    {
        char font_size_str[256];
        memcpy(font_size_str, size_str, size_str_len);
        font_size_str[size_str_len] = '\0';
        font_size = static_cast<float>(atof(font_size_str));
    }

    UpdateFont(font_size, guifont, static_cast<int>(font_str_len));
}

void Renderer::SetGuiOptions(mpack_node_t option_set)
{
    uint64_t option_set_length = mpack_node_array_length(option_set);

    for (uint64_t i = 1; i < option_set_length; ++i)
    {
        mpack_node_t name =
            mpack_node_array_at(mpack_node_array_at(option_set, i), 0);
        mpack_node_t value =
            mpack_node_array_at(mpack_node_array_at(option_set, i), 1);
        if (MPackMatchString(name, "guifont"))
        {
            const char *font_str = mpack_node_str(value);
            size_t strlen = mpack_node_strlen(value);
            this->UpdateGuiFont(font_str, strlen);

            // Send message to window in order to update nvim row/col count
            PostMessage(this->hwnd, WM_RENDERER_FONT_UPDATE, 0, 0);
        }
    }
}

void Renderer::ClearGrid()
{
    // Initialize all grid character to a space.
    for (int i = 0; i < this->grid_cols * this->grid_rows; ++i)
    {
        this->grid_chars[i] = L' ';
    }
    memset(this->grid_cell_properties, 0,
           this->grid_cols * this->grid_rows * sizeof(CellProperty));
    D2D1_RECT_F rect{.left = 0.0f,
                     .top = 0.0f,
                     .right = this->grid_cols * this->font_width,
                     .bottom = this->grid_rows * this->font_height};
    this->DrawBackgroundRect(rect, &this->hl_attribs[0]);
}

void Renderer::StartDraw()
{
    if (!this->draw_active)
    {
        WaitForSingleObjectEx(this->swapchain_wait_handle, 1000, true);

        this->d2d_context->SetTarget(this->d2d_target_bitmap);
        this->d2d_context->BeginDraw();
        this->d2d_context->SetTransform(D2D1::IdentityMatrix());
        this->draw_active = true;
    }
}

void Renderer::CopyFrontToBack()
{
    ID3D11Resource *front;
    ID3D11Resource *back;
    WIN_CHECK(this->dxgi_swapchain->GetBuffer(0, IID_PPV_ARGS(&back)));
    WIN_CHECK(this->dxgi_swapchain->GetBuffer(1, IID_PPV_ARGS(&front)));
    this->d3d_context->CopyResource(back, front);

    SafeRelease(&front);
    SafeRelease(&back);
}

void Renderer::FinishDraw()
{
    this->d2d_context->EndDraw();

    HRESULT hr = this->dxgi_swapchain->Present(0, 0);
    this->draw_active = false;

    this->CopyFrontToBack();

    // clear render target
    ID3D11RenderTargetView *null_views[] = {nullptr};
    this->d3d_context->OMSetRenderTargets(ARRAYSIZE(null_views), null_views,
                                          nullptr);
    this->d2d_context->SetTarget(nullptr);
    this->d3d_context->Flush();

    if (hr == DXGI_ERROR_DEVICE_REMOVED)
    {
        this->HandleDeviceLost();
    }
}

void Renderer::Redraw(mpack_node_t params)
{
    if (!dxgi_swapchain)
    {
        //
        // create swapchain resource
        //
        RECT client_rect;
        GetClientRect(hwnd, &client_rect);
        pixel_size.width =
            static_cast<uint32_t>(client_rect.right - client_rect.left);
        pixel_size.height =
            static_cast<uint32_t>(client_rect.bottom - client_rect.top);
        this->InitializeWindowDependentResources();
    }
    else
    {
        DXGI_SWAP_CHAIN_DESC desc;
        dxgi_swapchain->GetDesc(&desc);
        if (desc.BufferDesc.Width != pixel_size.width ||
            desc.BufferDesc.Height != pixel_size.height)
        {
            //
            // resize swapchain
            //
            this->InitializeWindowDependentResources();
        }
    }

    this->StartDraw();

    uint64_t redraw_commands_length = mpack_node_array_length(params);
    for (uint64_t i = 0; i < redraw_commands_length; ++i)
    {
        mpack_node_t redraw_command_arr = mpack_node_array_at(params, i);
        mpack_node_t redraw_command_name =
            mpack_node_array_at(redraw_command_arr, 0);

        if (MPackMatchString(redraw_command_name, "option_set"))
        {
            this->SetGuiOptions(redraw_command_arr);
        }
        if (MPackMatchString(redraw_command_name, "grid_resize"))
        {
            this->UpdateGridSize(redraw_command_arr);
        }
        if (MPackMatchString(redraw_command_name, "grid_clear"))
        {
            this->ClearGrid();
        }
        else if (MPackMatchString(redraw_command_name, "default_colors_set"))
        {
            this->UpdateDefaultColors(redraw_command_arr);
        }
        else if (MPackMatchString(redraw_command_name, "hl_attr_define"))
        {
            this->UpdateHighlightAttributes(redraw_command_arr);
        }
        else if (MPackMatchString(redraw_command_name, "grid_line"))
        {
            this->DrawGridLines(redraw_command_arr);
        }
        else if (MPackMatchString(redraw_command_name, "grid_cursor_goto"))
        {
            // If the old cursor position is still within the row bounds,
            // redraw the line to get rid of the cursor
            if (this->cursor.row < this->grid_rows)
            {
                this->DrawGridLine(this->cursor.row);
            }
            this->UpdateCursorPos(redraw_command_arr);
        }
        else if (MPackMatchString(redraw_command_name, "mode_info_set"))
        {
            this->UpdateCursorModeInfos(redraw_command_arr);
        }
        else if (MPackMatchString(redraw_command_name, "mode_change"))
        {
            // Redraw cursor if its inside the bounds
            if (this->cursor.row < this->grid_rows)
            {
                this->DrawGridLine(this->cursor.row);
            }
            this->UpdateCursorMode(redraw_command_arr);
        }
        else if (MPackMatchString(redraw_command_name, "busy_start"))
        {
            this->ui_busy = true;
            // Hide cursor while UI is busy
            if (this->cursor.row < this->grid_rows)
            {
                this->DrawGridLine(this->cursor.row);
            }
        }
        else if (MPackMatchString(redraw_command_name, "busy_stop"))
        {
            this->ui_busy = false;
        }
        else if (MPackMatchString(redraw_command_name, "grid_scroll"))
        {
            this->ScrollRegion(redraw_command_arr);
        }
        else if (MPackMatchString(redraw_command_name, "flush"))
        {
            if (!this->ui_busy)
            {
                this->DrawCursor();
            }
            this->DrawBorderRectangles();
            this->FinishDraw();
        }
    }
}

PixelSize Renderer::GridToPixelSize(int rows, int cols)
{
    int requested_width = static_cast<int>(ceilf(this->font_width) * cols);
    int requested_height = static_cast<int>(ceilf(this->font_height) * rows);

    // Adjust size to include title bar
    RECT adjusted_rect = {0, 0, requested_width, requested_height};
    AdjustWindowRect(&adjusted_rect, WS_OVERLAPPEDWINDOW, false);
    return PixelSize{.width = adjusted_rect.right - adjusted_rect.left,
                     .height = adjusted_rect.bottom - adjusted_rect.top};
}

GridSize Renderer::PixelsToGridSize(int width, int height)
{
    return ::GridSize{.rows = static_cast<int>(height / this->font_height),
                      .cols = static_cast<int>(width / this->font_width)};
}

GridPoint Renderer::CursorToGridPoint(int x, int y)
{
    return GridPoint{.row = static_cast<int>(y / this->font_height),
                     .col = static_cast<int>(x / this->font_width)};
}

GridSize Renderer::GridSize()
{
    return PixelsToGridSize(pixel_size.width, pixel_size.height);
}

bool Renderer::SetDpiScale(float current_dpi, int *pRows, int *pCols)
{
    dpi_scale = current_dpi / 96.0f;
    UpdateFont(last_requested_font_size);
    auto [rows, cols] = GridSize();
    *pRows = rows;
    *pCols = cols;
    return rows != grid_rows || cols != grid_cols;
}

bool Renderer::ResizeFont(float size, int *pRows, int *pCols)
{
    UpdateFont(last_requested_font_size + size);
    auto [rows, cols] = GridSize();
    *pRows = rows;
    *pCols = cols;
    return rows != grid_rows || cols != grid_cols;
}

HRESULT Renderer::DrawGlyphRun(
    float baseline_origin_x, float baseline_origin_y,
    DWRITE_MEASURING_MODE measuring_mode, DWRITE_GLYPH_RUN const *glyph_run,
    DWRITE_GLYPH_RUN_DESCRIPTION const *glyph_run_description,
    IUnknown *client_drawing_effect)
{
    HRESULT hr = S_OK;
    if (client_drawing_effect)
    {
        ComPtr<GlyphDrawingEffect> drawing_effect;
        client_drawing_effect->QueryInterface(
            __uuidof(GlyphDrawingEffect),
            reinterpret_cast<void **>(drawing_effect.ReleaseAndGetAddressOf()));
        drawing_effect_brush->SetColor(
            D2D1::ColorF(drawing_effect->text_color));
    }
    else
    {
        drawing_effect_brush->SetColor(
            D2D1::ColorF(this->hl_attribs[0].foreground));
    }

    DWRITE_GLYPH_IMAGE_FORMATS supported_formats =
        DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_CFF |
        DWRITE_GLYPH_IMAGE_FORMATS_COLR | DWRITE_GLYPH_IMAGE_FORMATS_SVG |
        DWRITE_GLYPH_IMAGE_FORMATS_PNG | DWRITE_GLYPH_IMAGE_FORMATS_JPEG |
        DWRITE_GLYPH_IMAGE_FORMATS_TIFF |
        DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8;

    IDWriteColorGlyphRunEnumerator1 *glyph_run_enumerator;
    hr = this->dwrite_factory->TranslateColorGlyphRun(
        D2D1_POINT_2F{.x = baseline_origin_x, .y = baseline_origin_y},
        glyph_run, glyph_run_description, supported_formats, measuring_mode,
        nullptr, 0, &glyph_run_enumerator);

    if (hr == DWRITE_E_NOCOLOR)
    {
        this->d2d_context->DrawGlyphRun(
            D2D1_POINT_2F{.x = baseline_origin_x, .y = baseline_origin_y},
            glyph_run, drawing_effect_brush, measuring_mode);
    }
    else
    {
        assert(!FAILED(hr));

        while (true)
        {
            BOOL has_run;
            WIN_CHECK(glyph_run_enumerator->MoveNext(&has_run));
            if (!has_run)
            {
                break;
            }

            DWRITE_COLOR_GLYPH_RUN1 const *color_run;
            WIN_CHECK(glyph_run_enumerator->GetCurrentRun(&color_run));

            D2D1_POINT_2F current_baseline_origin{
                .x = color_run->baselineOriginX,
                .y = color_run->baselineOriginY};

            switch (color_run->glyphImageFormat)
            {
            case DWRITE_GLYPH_IMAGE_FORMATS_PNG:
            case DWRITE_GLYPH_IMAGE_FORMATS_JPEG:
            case DWRITE_GLYPH_IMAGE_FORMATS_TIFF:
            case DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8:
            {
                this->d2d_context->DrawColorBitmapGlyphRun(
                    color_run->glyphImageFormat, current_baseline_origin,
                    &color_run->glyphRun, measuring_mode);
            }
            break;
            case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
            {
                this->d2d_context->DrawSvgGlyphRun(
                    current_baseline_origin, &color_run->glyphRun,
                    drawing_effect_brush, nullptr, 0, measuring_mode);
            }
            break;
            case DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE:
            case DWRITE_GLYPH_IMAGE_FORMATS_CFF:
            case DWRITE_GLYPH_IMAGE_FORMATS_COLR:
            default:
            {
                bool use_palette_color = color_run->paletteIndex != 0xFFFF;
                if (use_palette_color)
                {
                    temp_brush->SetColor(color_run->runColor);
                }

                this->d2d_context->PushAxisAlignedClip(
                    D2D1_RECT_F{
                        .left = current_baseline_origin.x,
                        .top = current_baseline_origin.y - this->font_ascent,
                        .right = current_baseline_origin.x +
                                 (color_run->glyphRun.glyphCount * 2 *
                                  this->font_width),
                        .bottom =
                            current_baseline_origin.y + this->font_descent,
                    },
                    D2D1_ANTIALIAS_MODE_ALIASED);
                this->d2d_context->DrawGlyphRun(
                    current_baseline_origin, &color_run->glyphRun,
                    color_run->glyphRunDescription,
                    use_palette_color ? temp_brush : drawing_effect_brush,
                    measuring_mode);
                this->d2d_context->PopAxisAlignedClip();
            }
            break;
            }
        }
    }

    return hr;
}

HRESULT Renderer::DrawUnderline(float baseline_origin_x,
                                float baseline_origin_y,
                                DWRITE_UNDERLINE const *underline,
                                IUnknown *client_drawing_effect)
{
    HRESULT hr = S_OK;
    if (client_drawing_effect)
    {
        ComPtr<GlyphDrawingEffect> drawing_effect;
        client_drawing_effect->QueryInterface(
            __uuidof(GlyphDrawingEffect),
            reinterpret_cast<void **>(drawing_effect.ReleaseAndGetAddressOf()));
        temp_brush->SetColor(D2D1::ColorF(drawing_effect->special_color));
    }
    else
    {
        temp_brush->SetColor(D2D1::ColorF(this->hl_attribs[0].special));
    }

    D2D1_RECT_F rect =
        D2D1_RECT_F{.left = baseline_origin_x,
                    .top = baseline_origin_y + underline->offset,
                    .right = baseline_origin_x + underline->width,
                    .bottom = baseline_origin_y + underline->offset +
                              max(underline->thickness, 1.0f)};

    this->d2d_context->FillRectangle(rect, temp_brush);
    return hr;
}

HRESULT Renderer::GetCurrentTransform(DWRITE_MATRIX *transform)
{
    this->d2d_context->GetTransform(
        reinterpret_cast<D2D1_MATRIX_3X2_F *>(transform));
    return S_OK;
}
