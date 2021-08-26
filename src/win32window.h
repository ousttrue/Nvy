#include <stdint.h>
#include <string>

class Win32Window {
  void *_instance = nullptr;
  void *_hwnd = nullptr;
  std::wstring _class_name;

public:
  ~Win32Window();

  void *Create(void *instance, const wchar_t *class_name,
               const wchar_t *window_title);

  uint64_t Proc(void *hwnd, uint32_t msg, uint64_t wparam, uint64_t lparam);
};
