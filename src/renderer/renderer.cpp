#include "renderer.h"
#include <tuple>
#include <vector>
using namespace Microsoft::WRL;

class GridImpl
{
    int _grid_rows = 0;
    int _grid_cols = 0;
    std::vector<wchar_t> _grid_chars;
    std::vector<CellProperty> _grid_cell_properties;

public:
    int Rows() const
    {
        return this->_grid_rows;
    }
    int Cols() const
    {
        return this->_grid_cols;
    }
    int Count() const
    {
        return this->_grid_cols * this->_grid_rows;
    }
    wchar_t *Chars()
    {
        return this->_grid_chars.data();
    }
    CellProperty *Props()
    {
        return this->_grid_cell_properties.data();
    }
    void Resize(int rows, int cols)
    {
        if (rows != _grid_rows || cols != _grid_cols)
        {
            _grid_cols = cols;
            _grid_rows = rows;

            auto count = Count();
            _grid_chars.resize(count);
            // Initialize all grid character to a space. An empty
            // grid cell is equivalent to a space in a text layout
            std::fill(_grid_chars.begin(), _grid_chars.end(), L' ');

            _grid_cell_properties.resize(count);
        }
    }
    void LineCopy(int left, int right, int src_row, int dst_row)
    {
        memcpy(&this->_grid_chars[dst_row * this->_grid_cols + left],
               &this->_grid_chars[src_row * this->_grid_cols + left],
               (right - left) * sizeof(wchar_t));

        memcpy(&this->_grid_cell_properties[dst_row * this->_grid_cols + left],
               &this->_grid_cell_properties[src_row * this->_grid_cols + left],
               (right - left) * sizeof(CellProperty));
    }
    void Clear()
    {
        // Initialize all grid character to a space.
        for (int i = 0; i < this->_grid_cols * this->_grid_rows; ++i)
        {
            this->_grid_chars[i] = L' ';
        }
        memset(this->_grid_cell_properties.data(), 0,
               this->_grid_cols * this->_grid_rows * sizeof(CellProperty));
    }
};

struct DECLSPEC_UUID("8d4d2884-e4d9-11ea-87d0-0242ac130003") GlyphDrawingEffect
    : public IUnknown
{
    ULONG _ref_count;
    uint32_t _text_color;
    uint32_t _special_color;

private:
    GlyphDrawingEffect(uint32_t text_color, uint32_t special_color)
        : _ref_count(1), _text_color(text_color), _special_color(special_color)
    {
    }

public:
    inline ULONG AddRef() noexcept override
    {
        return InterlockedIncrement(&_ref_count);
    }

    inline ULONG Release() noexcept override
    {
        ULONG new_count = InterlockedDecrement(&_ref_count);
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
    ULONG _ref_count;

private:
    GlyphRenderer() : _ref_count(1)
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
        return InterlockedIncrement(&_ref_count);
    }

    ULONG Release() noexcept override
    {
        ULONG new_count = InterlockedDecrement(&_ref_count);
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

class DWriteImpl
{
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
    DWriteImpl()
    {
    }
    ~DWriteImpl()
    {
    }

    static std::unique_ptr<DWriteImpl>
    Create(bool disable_ligatures, float linespace_factor, float monitor_dpi)
    {
        ComPtr<IDWriteFactory4> dwrite_factory;
        auto hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory4),
            reinterpret_cast<IUnknown **>(
                dwrite_factory.ReleaseAndGetAddressOf()));
        if (FAILED(hr))
        {
            return nullptr;
        }

        ComPtr<IDWriteTypography> dwrite_typography;
        if (disable_ligatures)
        {
            hr = dwrite_factory->CreateTypography(&dwrite_typography);
            if (FAILED(hr))
            {
                return nullptr;
            }

            hr = dwrite_typography->AddFontFeature(DWRITE_FONT_FEATURE{
                .nameTag = DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES,
                .parameter = 0});
            if (FAILED(hr))
            {
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

    void SetDpiScale(float current_dpi)
    {
        _dpi_scale = current_dpi / 96.0f;
        UpdateFont(_last_requested_font_size);
    }

    void ResizeFont(float size)
    {
        UpdateFont(_last_requested_font_size + size);
    }

    void UpdateFont(float font_size, const char *font_string = "",
                    int strlen = 0)
    {
        this->_dwrite_text_format.Reset();
        this->UpdateFontMetrics(font_size, font_string, strlen);
    }

    void UpdateFontMetrics(float font_size, const char *font_string, int strlen)
    {
        font_size = max(5.0f, min(font_size, 150.0f));
        this->_last_requested_font_size = font_size;

        ComPtr<IDWriteFontCollection> font_collection;
        WIN_CHECK(
            this->_dwrite_factory->GetSystemFontCollection(&font_collection));

        int wstrlen =
            MultiByteToWideChar(CP_UTF8, 0, font_string, strlen, 0, 0);
        if (wstrlen != 0 && wstrlen < MAX_FONT_LENGTH)
        {
            MultiByteToWideChar(CP_UTF8, 0, font_string, strlen, this->_font,
                                MAX_FONT_LENGTH - 1);
            this->_font[wstrlen] = L'\0';
        }

        uint32_t index;
        BOOL exists;
        font_collection->FindFamilyName(this->_font, &index, &exists);

        const wchar_t *fallback_font = L"Consolas";
        if (!exists)
        {
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
        WIN_CHECK(
            font_face->QueryInterface<IDWriteFontFace1>(&this->_font_face));

        this->_font_face->GetMetrics(&this->_font_metrics);

        uint16_t glyph_index;
        constexpr uint32_t codepoint = L'A';
        WIN_CHECK(
            this->_font_face->GetGlyphIndicesW(&codepoint, 1, &glyph_index));

        int32_t glyph_advance_in_em;
        WIN_CHECK(this->_font_face->GetDesignGlyphAdvances(
            1, &glyph_index, &glyph_advance_in_em));

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
        float frac_font_ascent =
            (this->_font_size * this->_font_metrics.ascent) /
            this->_font_metrics.designUnitsPerEm;
        float frac_font_descent =
            (this->_font_size * this->_font_metrics.descent) /
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
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            this->_font_size, L"en-us", &this->_dwrite_text_format));

        WIN_CHECK(this->_dwrite_text_format->SetLineSpacing(
            DWRITE_LINE_SPACING_METHOD_UNIFORM, this->_font_height,
            this->_font_ascent * this->_linespace_factor));
        WIN_CHECK(this->_dwrite_text_format->SetParagraphAlignment(
            DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
        WIN_CHECK(this->_dwrite_text_format->SetWordWrapping(
            DWRITE_WORD_WRAPPING_NO_WRAP));
    }

    float GetTextWidth(wchar_t *text, uint32_t length)
    {
        // Create dummy text format to hit test the width of the font
        ComPtr<IDWriteTextLayout> test_text_layout;
        WIN_CHECK(this->_dwrite_factory->CreateTextLayout(
            text, length, this->_dwrite_text_format.Get(), 0.0f, 0.0f,
            &test_text_layout));

        DWRITE_HIT_TEST_METRICS metrics;
        float _;
        WIN_CHECK(
            test_text_layout->HitTestTextPosition(0, 0, &_, &_, &metrics));
        return metrics.width;
    }

    ComPtr<IDWriteTextLayout1> GetTextLayout(const D2D1_RECT_F &rect,
                                             wchar_t *text, uint32_t length)
    {
        ComPtr<IDWriteTextLayout> temp_text_layout;
        WIN_CHECK(this->_dwrite_factory->CreateTextLayout(
            text, length, this->_dwrite_text_format.Get(),
            rect.right - rect.left, rect.bottom - rect.top, &temp_text_layout));
        ComPtr<IDWriteTextLayout1> text_layout;
        temp_text_layout.As(&text_layout);
        return text_layout;
    }

    void
    SetTypographyIfNotLigatures(const ComPtr<IDWriteTextLayout> &text_layout,
                                uint32_t length)
    {
        if (this->_disable_ligatures)
        {
            text_layout->SetTypography(
                this->_dwrite_typography.Get(),
                DWRITE_TEXT_RANGE{.startPosition = 0, .length = length});
        }
    }
};

class DeviceImpl
{
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
    static std::unique_ptr<DeviceImpl> Create()
    {
        uint32_t flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        // Force DirectX 11.1
        D3D_FEATURE_LEVEL d3d_feature_level;
        ComPtr<ID3D11Device> temp_device;
        ComPtr<ID3D11DeviceContext> temp_context;
        D3D_FEATURE_LEVEL feature_levels[] = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
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
        if (FAILED(hr))
        {
            assert(false);
        }

        return p;
    }
};

class SwapchainImpl
{
    ComPtr<IDXGISwapChain2> _dxgi_swapchain;
    HANDLE _swapchain_wait_handle = nullptr;
    DXGI_SWAP_CHAIN_DESC _desc;

public:
    static std::unique_ptr<SwapchainImpl>
    Create(const ComPtr<ID3D11Device2> &d3d_device, HWND hwnd)
    {
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
        WIN_CHECK(
            dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
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

    void Wait()
    {
        WaitForSingleObjectEx(_swapchain_wait_handle, 1000, true);
    }

    std::tuple<uint32_t, uint32_t> GetSize() const
    {
        return std::make_pair(_desc.BufferDesc.Width, _desc.BufferDesc.Height);
    }

    HRESULT Resize(uint32_t w, uint32_t h)
    {
        DXGI_SWAP_CHAIN_DESC desc;
        _dxgi_swapchain->GetDesc(&desc);
        if (desc.BufferDesc.Width == w && desc.BufferDesc.Height == h)
        {
            return S_OK;
        }

        HRESULT hr = this->_dxgi_swapchain->ResizeBuffers(
            2, w, h, DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
        return hr;
    }

    ComPtr<IDXGISurface2> GetBackbuffer()
    {
        ComPtr<IDXGISurface2> dxgi_backbuffer;
        WIN_CHECK(this->_dxgi_swapchain->GetBuffer(
            0, IID_PPV_ARGS(&dxgi_backbuffer)));
        return dxgi_backbuffer;
    }

    HRESULT
    PresentCopyFrontToBack(const ComPtr<ID3D11DeviceContext2> &d3d_context)
    {
        HRESULT hr = this->_dxgi_swapchain->Present(0, 0);
        if (FAILED(hr))
        {
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

void Renderer::InitializeWindowDependentResources()
{
    if (this->_swapchain)
    {
        uint32_t w, h;
        std::tie(w, h) = this->_swapchain->GetSize();
        if (_pixel_size.width == w && _pixel_size.height == h)
        {
            // not resized. use same bitmap
            return;
        }
        HRESULT hr =
            this->_swapchain->Resize(_pixel_size.width, _pixel_size.height);
        if (hr == DXGI_ERROR_DEVICE_REMOVED)
        {
            this->HandleDeviceLost();
        }
    }
    else
    {
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

void Renderer::HandleDeviceLost()
{
    _device.reset();
    _swapchain.reset();
    _d2d_target_bitmap.Reset();

    _device = DeviceImpl::Create();
    this->UpdateFont(DEFAULT_FONT_SIZE, DEFAULT_FONT,
                     static_cast<int>(strlen(DEFAULT_FONT)));
}

Renderer::Renderer(HWND hwnd, bool disable_ligatures, float linespace_factor,
                   float monitor_dpi)
    : _dwrite(
          DWriteImpl::Create(disable_ligatures, linespace_factor, monitor_dpi)),
      _grid(new GridImpl)
{
    this->_hwnd = hwnd;
    this->_hl_attribs.resize(MAX_HIGHLIGHT_ATTRIBS);

    this->HandleDeviceLost();
}

Renderer::~Renderer()
{
}

void Renderer::Attach()
{
    RECT client_rect;
    GetClientRect(this->_hwnd, &client_rect);
    _pixel_size.width =
        static_cast<uint32_t>(client_rect.right - client_rect.left);
    _pixel_size.height =
        static_cast<uint32_t>(client_rect.bottom - client_rect.top);
}

void Renderer::Resize(uint32_t width, uint32_t height)
{
    _pixel_size.width = width;
    _pixel_size.height = height;
}

void Renderer::UpdateDefaultColors(mpack_node_t default_colors)
{
    size_t default_colors_arr_length = mpack_node_array_length(default_colors);

    for (size_t i = 1; i < default_colors_arr_length; ++i)
    {
        mpack_node_t color_arr = mpack_node_array_at(default_colors, i);

        // Default colors occupy the first index of the highlight attribs array
        this->_hl_attribs[0].foreground = static_cast<uint32_t>(
            mpack_node_array_at(color_arr, 0).data->value.u);
        this->_hl_attribs[0].background = static_cast<uint32_t>(
            mpack_node_array_at(color_arr, 1).data->value.u);
        this->_hl_attribs[0].special = static_cast<uint32_t>(
            mpack_node_array_at(color_arr, 2).data->value.u);
        this->_hl_attribs[0].flags = 0;
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

        const auto SetColor = [&](const char *name, uint32_t *color)
        {
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
        SetColor("foreground", &this->_hl_attribs[attrib_index].foreground);
        SetColor("background", &this->_hl_attribs[attrib_index].background);
        SetColor("special", &this->_hl_attribs[attrib_index].special);

        const auto SetFlag =
            [&](const char *flag_name, HighlightAttributeFlags flag)
        {
            mpack_node_t flag_node =
                mpack_node_map_cstr_optional(attrib_map, flag_name);
            if (!mpack_node_is_missing(flag_node))
            {
                if (flag_node.data->value.b)
                {
                    this->_hl_attribs[attrib_index].flags |= flag;
                }
                else
                {
                    this->_hl_attribs[attrib_index].flags &= ~flag;
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
                   ? this->_hl_attribs[0].background
                   : hl_attribs->background;
    }
    else
    {
        return hl_attribs->foreground == DEFAULT_COLOR
                   ? this->_hl_attribs[0].foreground
                   : hl_attribs->foreground;
    }
}

uint32_t Renderer::CreateBackgroundColor(HighlightAttributes *hl_attribs)
{
    if (hl_attribs->flags & HL_ATTRIB_REVERSE)
    {
        return hl_attribs->foreground == DEFAULT_COLOR
                   ? this->_hl_attribs[0].foreground
                   : hl_attribs->foreground;
    }
    else
    {
        return hl_attribs->background == DEFAULT_COLOR
                   ? this->_hl_attribs[0].background
                   : hl_attribs->background;
    }
}

uint32_t Renderer::CreateSpecialColor(HighlightAttributes *hl_attribs)
{
    return hl_attribs->special == DEFAULT_COLOR ? this->_hl_attribs[0].special
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
    _device->_d2d_background_rect_brush->SetColor(D2D1::ColorF(color));
    _device->_d2d_context->FillRectangle(
        rect, _device->_d2d_background_rect_brush.Get());
}

D2D1_RECT_F Renderer::GetCursorForegroundRect(D2D1_RECT_F cursor_bg_rect)
{
    if (this->_cursor.mode_info)
    {
        switch (this->_cursor.mode_info->shape)
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
    auto text_layout = _dwrite->GetTextLayout(rect, text, length);
    this->ApplyHighlightAttributes(hl_attribs, text_layout.Get(), 0, 1);

    _device->_d2d_context->PushAxisAlignedClip(rect,
                                               D2D1_ANTIALIAS_MODE_ALIASED);
    text_layout->Draw(this, _device->_glyph_renderer.Get(), rect.left,
                      rect.top);
    text_layout->Release();
    _device->_d2d_context->PopAxisAlignedClip();
}

void Renderer::DrawGridLine(int row)
{
    auto cols = _grid->Cols();
    int base = row * cols;

    D2D1_RECT_F rect{.left = 0.0f,
                     .top = row * _dwrite->_font_height,
                     .right = cols * _dwrite->_font_width,
                     .bottom =
                         (row * _dwrite->_font_height) + _dwrite->_font_height};

    auto text_layout =
        _dwrite->GetTextLayout(rect, &_grid->Chars()[base], cols);

    uint16_t hl_attrib_id = _grid->Props()[base].hl_attrib_id;
    int col_offset = 0;
    for (int i = 0; i < cols; ++i)
    {
        // Add spacing for wide chars
        if (_grid->Props()[base + i].is_wide_char)
        {
            float char_width =
                _dwrite->GetTextWidth(&_grid->Chars()[base + i], 2);
            DWRITE_TEXT_RANGE range{.startPosition = static_cast<uint32_t>(i),
                                    .length = 1};
            text_layout->SetCharacterSpacing(
                0, (_dwrite->_font_width * 2) - char_width, 0, range);
        }

        // Add spacing for unicode chars. These characters are still single char
        // width, but some of them by default will take up a bit more or less,
        // leading to issues. So we realign them here.
        else if (_grid->Chars()[base + i] > 0xFF)
        {
            float char_width =
                _dwrite->GetTextWidth(&_grid->Chars()[base + i], 1);
            if (abs(char_width - _dwrite->_font_width) > 0.01f)
            {
                DWRITE_TEXT_RANGE range{
                    .startPosition = static_cast<uint32_t>(i), .length = 1};
                text_layout->SetCharacterSpacing(
                    0, _dwrite->_font_width - char_width, 0, range);
            }
        }

        // Check if the attributes change,
        // if so draw until this point and continue with the new attributes
        if (_grid->Props()[base + i].hl_attrib_id != hl_attrib_id)
        {
            D2D1_RECT_F bg_rect{.left = col_offset * _dwrite->_font_width,
                                .top = row * _dwrite->_font_height,
                                .right =
                                    col_offset * _dwrite->_font_width +
                                    _dwrite->_font_width * (i - col_offset),
                                .bottom = (row * _dwrite->_font_height) +
                                          _dwrite->_font_height};
            this->DrawBackgroundRect(bg_rect, &this->_hl_attribs[hl_attrib_id]);
            this->ApplyHighlightAttributes(&this->_hl_attribs[hl_attrib_id],
                                           text_layout.Get(), col_offset, i);

            hl_attrib_id = _grid->Props()[base + i].hl_attrib_id;
            col_offset = i;
        }
    }

    // Draw the remaining columns, there is always atleast the last column to
    // draw, but potentially more in case the last X columns share the same
    // hl_attrib
    D2D1_RECT_F last_rect = rect;
    last_rect.left = col_offset * _dwrite->_font_width;
    this->DrawBackgroundRect(last_rect, &this->_hl_attribs[hl_attrib_id]);
    this->ApplyHighlightAttributes(&this->_hl_attribs[hl_attrib_id],
                                   text_layout.Get(), col_offset, cols);

    _device->_d2d_context->PushAxisAlignedClip(rect,
                                               D2D1_ANTIALIAS_MODE_ALIASED);
    _dwrite->SetTypographyIfNotLigatures(text_layout,
                                         static_cast<uint32_t>(cols));
    text_layout->Draw(this, _device->_glyph_renderer.Get(), 0.0f, rect.top);
    _device->_d2d_context->PopAxisAlignedClip();
}

void Renderer::DrawGridLines(mpack_node_t grid_lines)
{
    int grid_size = _grid->Count();
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

                int offset = row * _grid->Cols() + col_offset;
                _grid->Props()[offset].is_wide_char = true;
                _grid->Props()[offset].hl_attrib_id = hl_attrib_id;
                _grid->Props()[offset + 1].hl_attrib_id = hl_attrib_id;

                int wstrlen = MultiByteToWideChar(CP_UTF8, 0, str, strlen,
                                                  &_grid->Chars()[offset],
                                                  grid_size - offset);
                assert(wstrlen == 1 || wstrlen == 2);

                if (wstrlen == 1)
                {
                    _grid->Chars()[offset + 1] = L'\0';
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

            int offset = row * _grid->Cols() + col_offset;
            int wstrlen = 0;
            for (int k = 0; k < repeat; ++k)
            {
                int idx = offset + (k * wstrlen);
                wstrlen =
                    MultiByteToWideChar(CP_UTF8, 0, str, strlen,
                                        &_grid->Chars()[idx], grid_size - idx);
            }

            int wstrlen_with_repetitions = wstrlen * repeat;
            for (int k = 0; k < wstrlen_with_repetitions; ++k)
            {
                _grid->Props()[offset + k].hl_attrib_id = hl_attrib_id;
                _grid->Props()[offset + k].is_wide_char = false;
            }

            col_offset += wstrlen_with_repetitions;
        }

        this->DrawGridLine(row);
    }
}

void Renderer::DrawCursor()
{
    int cursor_grid_offset =
        this->_cursor.row * _grid->Cols() + this->_cursor.col;

    int double_width_char_factor = 1;
    if (cursor_grid_offset < _grid->Count() &&
        _grid->Props()[cursor_grid_offset].is_wide_char)
    {
        double_width_char_factor += 1;
    }

    HighlightAttributes cursor_hl_attribs =
        this->_hl_attribs[this->_cursor.mode_info->hl_attrib_id];
    if (this->_cursor.mode_info->hl_attrib_id == 0)
    {
        cursor_hl_attribs.flags ^= HL_ATTRIB_REVERSE;
    }

    D2D1_RECT_F cursor_rect{
        .left = this->_cursor.col * _dwrite->_font_width,
        .top = this->_cursor.row * _dwrite->_font_height,
        .right = this->_cursor.col * _dwrite->_font_width +
                 _dwrite->_font_width * double_width_char_factor,
        .bottom = (this->_cursor.row * _dwrite->_font_height) +
                  _dwrite->_font_height};
    D2D1_RECT_F cursor_fg_rect = this->GetCursorForegroundRect(cursor_rect);
    this->DrawBackgroundRect(cursor_fg_rect, &cursor_hl_attribs);

    if (this->_cursor.mode_info->shape == CursorShape::Block)
    {
        this->DrawHighlightedText(cursor_fg_rect,
                                  &_grid->Chars()[cursor_grid_offset],
                                  double_width_char_factor, &cursor_hl_attribs);
    }
}

void Renderer::UpdateGridSize(mpack_node_t grid_resize)
{
    mpack_node_t grid_resize_params = mpack_node_array_at(grid_resize, 1);
    int grid_cols = MPackIntFromArray(grid_resize_params, 1);
    int grid_rows = MPackIntFromArray(grid_resize_params, 2);

    _grid->Resize(grid_rows, grid_cols);
}

void Renderer::UpdateCursorPos(mpack_node_t cursor_goto)
{
    mpack_node_t cursor_goto_params = mpack_node_array_at(cursor_goto, 1);
    this->_cursor.row = MPackIntFromArray(cursor_goto_params, 1);
    this->_cursor.col = MPackIntFromArray(cursor_goto_params, 2);
}

void Renderer::UpdateCursorMode(mpack_node_t mode_change)
{
    mpack_node_t mode_change_params = mpack_node_array_at(mode_change, 1);
    this->_cursor.mode_info =
        &this->_cursor_mode_infos[mpack_node_array_at(mode_change_params, 1)
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

        this->_cursor_mode_infos[i].shape = CursorShape::None;
        mpack_node_t cursor_shape =
            mpack_node_map_cstr_optional(mode_info_map, "cursor_shape");
        if (!mpack_node_is_missing(cursor_shape))
        {
            const char *cursor_shape_str = mpack_node_str(cursor_shape);
            size_t strlen = mpack_node_strlen(cursor_shape);
            if (!strncmp(cursor_shape_str, "block", strlen))
            {
                this->_cursor_mode_infos[i].shape = CursorShape::Block;
            }
            else if (!strncmp(cursor_shape_str, "vertical", strlen))
            {
                this->_cursor_mode_infos[i].shape = CursorShape::Vertical;
            }
            else if (!strncmp(cursor_shape_str, "horizontal", strlen))
            {
                this->_cursor_mode_infos[i].shape = CursorShape::Horizontal;
            }
        }

        this->_cursor_mode_infos[i].hl_attrib_id = 0;
        mpack_node_t hl_attrib_index =
            mpack_node_map_cstr_optional(mode_info_map, "attr_id");
        if (!mpack_node_is_missing(hl_attrib_index))
        {
            this->_cursor_mode_infos[i].hl_attrib_id =
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

        _grid->LineCopy(left, right, i, target_row);

        // Sadly I have given up on making use of IDXGISwapChain1::Present1
        // scroll_rects or bitmap copies. The former seems insufficient for
        // nvim since it can require multiple scrolls per frame, the latter
        // I can't seem to make work with the FLIP_SEQUENTIAL swapchain model.
        // Thus we fall back to drawing the appropriate scrolled grid lines
        this->DrawGridLine(target_row);
    }

    // Redraw the line which the cursor has moved to, as it is no
    // longer guaranteed that the cursor is still there
    int cursor_row = this->_cursor.row - rows;
    if (cursor_row >= 0 && cursor_row < _grid->Rows())
    {
        this->DrawGridLine(cursor_row);
    }
}

void Renderer::DrawBorderRectangles()
{
    float left_border = _dwrite->_font_width * _grid->Cols();
    float top_border = _dwrite->_font_height * _grid->Rows();

    if (left_border != static_cast<float>(this->_pixel_size.width))
    {
        D2D1_RECT_F vertical_rect{
            .left = left_border,
            .top = 0.0f,
            .right = static_cast<float>(this->_pixel_size.width),
            .bottom = static_cast<float>(this->_pixel_size.height)};
        this->DrawBackgroundRect(vertical_rect, &this->_hl_attribs[0]);
    }

    if (top_border != static_cast<float>(this->_pixel_size.height))
    {
        D2D1_RECT_F horizontal_rect{
            .left = 0.0f,
            .top = top_border,
            .right = static_cast<float>(this->_pixel_size.width),
            .bottom = static_cast<float>(this->_pixel_size.height)};
        this->DrawBackgroundRect(horizontal_rect, &this->_hl_attribs[0]);
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
            PostMessage(this->_hwnd, WM_RENDERER_FONT_UPDATE, 0, 0);
        }
    }
}

void Renderer::ClearGrid()
{
    _grid->Clear();
    D2D1_RECT_F rect{.left = 0.0f,
                     .top = 0.0f,
                     .right = _grid->Cols() * _dwrite->_font_width,
                     .bottom = _grid->Rows() * _dwrite->_font_height};
    this->DrawBackgroundRect(rect, &this->_hl_attribs[0]);
}

void Renderer::StartDraw()
{
    if (!this->_draw_active)
    {
        _swapchain->Wait();

        _device->_d2d_context->SetTarget(this->_d2d_target_bitmap.Get());
        _device->_d2d_context->BeginDraw();
        _device->_d2d_context->SetTransform(D2D1::IdentityMatrix());
        this->_draw_active = true;
    }
}

void Renderer::FinishDraw()
{
    _device->_d2d_context->EndDraw();

    auto hr = _swapchain->PresentCopyFrontToBack(_device->_d3d_context);

    this->_draw_active = false;

    // clear render target
    ID3D11RenderTargetView *null_views[] = {nullptr};
    _device->_d3d_context->OMSetRenderTargets(ARRAYSIZE(null_views), null_views,
                                              nullptr);
    _device->_d2d_context->SetTarget(nullptr);
    _device->_d3d_context->Flush();

    if (hr == DXGI_ERROR_DEVICE_REMOVED)
    {
        this->HandleDeviceLost();
    }
}

void Renderer::Redraw(mpack_node_t params)
{
    this->InitializeWindowDependentResources();

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
            if (this->_cursor.row < _grid->Rows())
            {
                this->DrawGridLine(this->_cursor.row);
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
            if (this->_cursor.row < _grid->Rows())
            {
                this->DrawGridLine(this->_cursor.row);
            }
            this->UpdateCursorMode(redraw_command_arr);
        }
        else if (MPackMatchString(redraw_command_name, "busy_start"))
        {
            this->_ui_busy = true;
            // Hide cursor while UI is busy
            if (this->_cursor.row < _grid->Rows())
            {
                this->DrawGridLine(this->_cursor.row);
            }
        }
        else if (MPackMatchString(redraw_command_name, "busy_stop"))
        {
            this->_ui_busy = false;
        }
        else if (MPackMatchString(redraw_command_name, "grid_scroll"))
        {
            this->ScrollRegion(redraw_command_arr);
        }
        else if (MPackMatchString(redraw_command_name, "flush"))
        {
            if (!this->_ui_busy)
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
    int requested_width = static_cast<int>(ceilf(_dwrite->_font_width) * cols);
    int requested_height =
        static_cast<int>(ceilf(_dwrite->_font_height) * rows);

    // Adjust size to include title bar
    RECT adjusted_rect = {0, 0, requested_width, requested_height};
    AdjustWindowRect(&adjusted_rect, WS_OVERLAPPEDWINDOW, false);
    return PixelSize{.width = adjusted_rect.right - adjusted_rect.left,
                     .height = adjusted_rect.bottom - adjusted_rect.top};
}

GridSize Renderer::PixelsToGridSize(int width, int height)
{
    return ::GridSize{.rows = static_cast<int>(height / _dwrite->_font_height),
                      .cols = static_cast<int>(width / _dwrite->_font_width)};
}

GridPoint Renderer::CursorToGridPoint(int x, int y)
{
    return GridPoint{.row = static_cast<int>(y / _dwrite->_font_height),
                     .col = static_cast<int>(x / _dwrite->_font_width)};
}

GridSize Renderer::GridSize()
{
    return PixelsToGridSize(_pixel_size.width, _pixel_size.height);
}

bool Renderer::SetDpiScale(float current_dpi, int *pRows, int *pCols)
{
    _dwrite->SetDpiScale(current_dpi);
    auto [rows, cols] = GridSize();
    *pRows = rows;
    *pCols = cols;
    return rows != _grid->Rows() || cols != _grid->Cols();
}

bool Renderer::ResizeFont(float size, int *pRows, int *pCols)
{
    _dwrite->ResizeFont(size);
    auto [rows, cols] = GridSize();
    *pRows = rows;
    *pCols = cols;
    return rows != _grid->Rows() || cols != _grid->Cols();
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
        _device->_drawing_effect_brush->SetColor(
            D2D1::ColorF(drawing_effect->_text_color));
    }
    else
    {
        _device->_drawing_effect_brush->SetColor(
            D2D1::ColorF(this->_hl_attribs[0].foreground));
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

    if (hr == DWRITE_E_NOCOLOR)
    {
        _device->_d2d_context->DrawGlyphRun(
            D2D1_POINT_2F{.x = baseline_origin_x, .y = baseline_origin_y},
            glyph_run, _device->_drawing_effect_brush.Get(), measuring_mode);
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
                _device->_d2d_context->DrawColorBitmapGlyphRun(
                    color_run->glyphImageFormat, current_baseline_origin,
                    &color_run->glyphRun, measuring_mode);
            }
            break;
            case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
            {
                _device->_d2d_context->DrawSvgGlyphRun(
                    current_baseline_origin, &color_run->glyphRun,
                    _device->_drawing_effect_brush.Get(), nullptr, 0,
                    measuring_mode);
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
                    _device->_temp_brush->SetColor(color_run->runColor);
                }

                _device->_d2d_context->PushAxisAlignedClip(
                    D2D1_RECT_F{
                        .left = current_baseline_origin.x,
                        .top =
                            current_baseline_origin.y - _dwrite->_font_ascent,
                        .right = current_baseline_origin.x +
                                 (color_run->glyphRun.glyphCount * 2 *
                                  _dwrite->_font_width),
                        .bottom =
                            current_baseline_origin.y + _dwrite->_font_descent,
                    },
                    D2D1_ANTIALIAS_MODE_ALIASED);
                _device->_d2d_context->DrawGlyphRun(
                    current_baseline_origin, &color_run->glyphRun,
                    color_run->glyphRunDescription,
                    use_palette_color ? _device->_temp_brush.Get()
                                      : _device->_drawing_effect_brush.Get(),
                    measuring_mode);
                _device->_d2d_context->PopAxisAlignedClip();
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
        _device->_temp_brush->SetColor(
            D2D1::ColorF(drawing_effect->_special_color));
    }
    else
    {
        _device->_temp_brush->SetColor(
            D2D1::ColorF(this->_hl_attribs[0].special));
    }

    D2D1_RECT_F rect =
        D2D1_RECT_F{.left = baseline_origin_x,
                    .top = baseline_origin_y + underline->offset,
                    .right = baseline_origin_x + underline->width,
                    .bottom = baseline_origin_y + underline->offset +
                              max(underline->thickness, 1.0f)};

    _device->_d2d_context->FillRectangle(rect, _device->_temp_brush.Get());
    return hr;
}

HRESULT Renderer::GetCurrentTransform(DWRITE_MATRIX *transform)
{
    _device->_d2d_context->GetTransform(
        reinterpret_cast<D2D1_MATRIX_3X2_F *>(transform));
    return S_OK;
}

void Renderer::UpdateFont(float font_size, const char *font_string, int strlen)
{
    _dwrite->UpdateFont(font_size, font_string, strlen);
}
