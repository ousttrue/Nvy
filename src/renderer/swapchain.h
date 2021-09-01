#pragma once
#include <d3d11_2.h>
#include <dxgi1_3.h>
#include <memory>
#include <wrl/client.h>

class Swapchain {
  template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;
  ComPtr<IDXGISwapChain2> _dxgi_swapchain;
  HANDLE _swapchain_wait_handle = nullptr;
  DXGI_SWAP_CHAIN_DESC _desc;

  Swapchain(const ComPtr<IDXGISwapChain2> &swapchain);

public:
  ~Swapchain() {}
  static std::unique_ptr<Swapchain>
  Create(const ComPtr<ID3D11Device2> &d3d_device, HWND hwnd);
  HRESULT Resize(uint32_t w, uint32_t h);
  std::tuple<uint32_t, uint32_t> GetSize() const {
    return std::make_pair(_desc.BufferDesc.Width, _desc.BufferDesc.Height);
  }
  ComPtr<IDXGISurface2> GetBackbuffer();
  void Wait();
  HRESULT
  PresentCopyFrontToBack(const ComPtr<ID3D11DeviceContext2> &d3d_context);
};
