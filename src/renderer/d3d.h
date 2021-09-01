#pragma once
#include <d3d11_2.h>
#include <memory>
#include <wrl/client.h>

class D3D {
  template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;
  ComPtr<ID3D11Device2> _device;
  ComPtr<ID3D11DeviceContext2> _context;

  D3D(){}
public:
  ~D3D(){}
  static std::unique_ptr<D3D> Create();
  const ComPtr<ID3D11Device2> &Device() const { return _device; }
  const ComPtr<ID3D11DeviceContext2> &Context() const { return _context; }
};
