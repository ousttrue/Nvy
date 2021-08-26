#include "window.h"

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                   LPARAM lparam) {
  if (msg == WM_CREATE) {
    LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
    SetWindowLongPtr(hwnd, GWLP_USERDATA,
                     reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
    return 0;
  }

  auto w = reinterpret_cast<Window *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  return w->Proc(hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK Window::Proc(HWND hwnd, UINT msg, WPARAM wparam,
                              LPARAM lparam) {
  switch (msg) {
  case WM_DESTROY: {
    PostQuitMessage(0);
    return 0;
  }
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}

HWND Window::Create(HINSTANCE instance, const wchar_t *class_name,
                    const wchar_t *title) {
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = class_name;
  if (!RegisterClassExW(&wc)) {
    return nullptr;
  }

  _hwnd =
      CreateWindowExW(WS_EX_ACCEPTFILES, class_name, title, WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                      CW_USEDEFAULT, nullptr, nullptr, instance, this);
  return _hwnd;
}

bool Window::NewFrame() {
  MSG msg;
  if (!GetMessage(&msg, NULL, 0, 0)) {
    return false;
  }

  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return true;
}
