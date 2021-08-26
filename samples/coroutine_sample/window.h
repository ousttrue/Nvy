#pragma once
#include <Windows.h>

class Window {
  HWND _hwnd = nullptr;

public:
  LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  HWND Create(HINSTANCE instance, const wchar_t *class_name,
              const wchar_t *title);
  bool NewFrame();
};
