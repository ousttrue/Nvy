#include "d3d.h"

std::unique_ptr<D3D> D3D::Create() {
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
  auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                              feature_levels, ARRAYSIZE(feature_levels),
                              D3D11_SDK_VERSION, &temp_device,
                              &d3d_feature_level, &temp_context);
  if (FAILED(hr)) {
    return nullptr;
  }

  auto p = std::unique_ptr<D3D>(new D3D);

  hr = temp_device.As(&p->_device);
  if (FAILED(hr)) {
    return nullptr;
  }

  hr = temp_context.As(&p->_context);
  if (FAILED(hr)) {
    return nullptr;
  }

  return p;
}
