#include <functional>
#include <stdint.h>
#include <string>

using on_int2_t = std::function<void(int, int)>;
using on_input_t = std::function<void(std::string_view)>;
using on_char_t = std::function<void(wchar_t)>;
using on_void_t = std::function<void(void)>;

class Win32Window {
  void *_instance = nullptr;
  void *_hwnd = nullptr;
  std::wstring _class_name;

  bool _dead_char_pending = false;

public:
  ~Win32Window();
  on_int2_t _on_resize = [](auto, auto) {};
  on_input_t _on_input;
  on_char_t _on_char;
  on_char_t _on_sys_char;
  on_void_t _on_toggle_screen;
  on_input_t _on_modified_input;
  on_int2_t _on_mouse_left_drag;
  on_int2_t _on_mouse_middle_drag;
  on_int2_t _on_mouse_right_drag;
  on_int2_t _on_mouse_left_down;
  on_int2_t _on_mouse_left_release;
  on_int2_t _on_mouse_middle_down;
  on_int2_t _on_mouse_middle_release;
  on_int2_t _on_mouse_right_down;
  on_int2_t _on_mouse_right_release;

  void *Create(void *instance, const wchar_t *class_name,
               const wchar_t *window_title);

  uint64_t Proc(void *hwnd, uint32_t msg, uint64_t wparam, uint64_t lparam);

  bool Loop();
  void ToggleFullscreen();
  void Resize(int w, int h);
};
