#include "swapchain.h"
template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

Swapchain::Swapchain(const ComPtr<IDXGISwapChain2> &swapchain)
    : _dxgi_swapchain(swapchain) {
  _swapchain_wait_handle = _dxgi_swapchain->GetFrameLatencyWaitableObject();
  _dxgi_swapchain->GetDesc(&_desc);
}

std::unique_ptr<Swapchain>
Swapchain::Create(const ComPtr<ID3D11Device2> &d3d_device, HWND hwnd) {
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
  auto hr = d3d_device.As(&dxgi_device);
  if (FAILED(hr)) {
    return nullptr;
  }

  ComPtr<IDXGIAdapter> dxgi_adapter;
  hr = dxgi_device->GetAdapter(&dxgi_adapter);
  if (FAILED(hr)) {
    return nullptr;
  }

  ComPtr<IDXGIFactory2> dxgi_factory;
  hr = dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));
  if (FAILED(hr)) {
    return nullptr;
  }

  ComPtr<IDXGISwapChain1> dxgi_swapchain_temp;
  hr = dxgi_factory->CreateSwapChainForHwnd(d3d_device.Get(), hwnd,
                                            &swapchain_desc, nullptr, nullptr,
                                            &dxgi_swapchain_temp);
  if (FAILED(hr)) {
    return nullptr;
  }

  hr = dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
  if (FAILED(hr)) {
    return nullptr;
  }

  ComPtr<IDXGISwapChain2> dxgi_swapchain;
  hr = dxgi_swapchain_temp.As(&dxgi_swapchain);
  if (FAILED(hr)) {
    return nullptr;
  }

  hr = dxgi_swapchain->SetMaximumFrameLatency(1);
  if (FAILED(hr)) {
    return nullptr;
  }

  return std::unique_ptr<Swapchain>(new Swapchain(dxgi_swapchain));
}

HRESULT Swapchain::Resize(uint32_t w, uint32_t h) {
  DXGI_SWAP_CHAIN_DESC desc;
  _dxgi_swapchain->GetDesc(&desc);
  if (desc.BufferDesc.Width == w && desc.BufferDesc.Height == h) {
    return S_OK;
  }

  HRESULT hr = _dxgi_swapchain->ResizeBuffers(
      2, w, h, DXGI_FORMAT_B8G8R8A8_UNORM,
      DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
          DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
  return hr;
}

ComPtr<IDXGISurface2> Swapchain::GetBackbuffer() {
  ComPtr<IDXGISurface2> dxgi_backbuffer;
  auto hr = _dxgi_swapchain->GetBuffer(0, IID_PPV_ARGS(&dxgi_backbuffer));
  if (FAILED(hr)) {
    return nullptr;
  }
  return dxgi_backbuffer;
}

void Swapchain::Wait() {
  WaitForSingleObjectEx(_swapchain_wait_handle, 1000, true);
}

HRESULT
Swapchain::PresentCopyFrontToBack(
    const ComPtr<ID3D11DeviceContext2> &d3d_context) {
  HRESULT hr = _dxgi_swapchain->Present(0, 0);
  if (FAILED(hr)) {
    return hr;
  }

  ComPtr<ID3D11Resource> back;
  hr = _dxgi_swapchain->GetBuffer(0, IID_PPV_ARGS(&back));
  if (FAILED(hr)) {
    return hr;
  }

  ComPtr<ID3D11Resource> front;
  hr = _dxgi_swapchain->GetBuffer(1, IID_PPV_ARGS(&front));
  if (FAILED(hr)) {
    return hr;
  }

  d3d_context->CopyResource(back.Get(), front.Get());
  return S_OK;
}
