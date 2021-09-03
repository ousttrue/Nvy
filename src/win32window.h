#include <functional>
#include <nvim_input.h>
#include <nvim_win32_key_processor.h>
#include <stdint.h>
#include <string>

using on_int2_t = std::function<void(int, int)>;
using on_input_t = std::function<void(const Nvim::InputEvent &)>;
using on_mouse_t = std::function<void(const Nvim::MouseEvent &)>;
using on_drop_file_t = std::function<void(const wchar_t *file)>;

class Win32Window {
  void *_instance = nullptr;
  void *_hwnd = nullptr;
  std::wstring _class_name;

  NvimWin32KeyProcessor _nvim_Key;

public:
  ~Win32Window();
  on_int2_t _on_resize = [](auto, auto) {};
  on_input_t _on_input;
  on_mouse_t _on_mouse;
  on_drop_file_t _on_drop_file;

  void *Create(void *instance, const wchar_t *class_name,
               const wchar_t *window_title);

  uint64_t Proc(void *hwnd, uint32_t msg, uint64_t wparam, uint64_t lparam);

  bool Loop();
  void ToggleFullscreen();
  void Resize(int w, int h);
  std::tuple<int, int> Size() const;
  uint32_t GetMonitorDpi() const;
};
