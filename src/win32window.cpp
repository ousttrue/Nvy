#include "win32window.h"
#include <Windows.h>

static const char *GetProcessKey(int virtual_key) {
  switch (virtual_key) {
  case VK_BACK:
    return "BS";
  case VK_TAB:
    return "Tab";
  case VK_RETURN:
    return "CR";
  case VK_ESCAPE:
    return "Esc";
  case VK_PRIOR:
    return "PageUp";
  case VK_NEXT:
    return "PageDown";
  case VK_HOME:
    return "Home";
  case VK_END:
    return "End";
  case VK_LEFT:
    return "Left";
  case VK_UP:
    return "Up";
  case VK_RIGHT:
    return "Right";
  case VK_DOWN:
    return "Down";
  case VK_INSERT:
    return "Insert";
  case VK_DELETE:
    return "Del";
  case VK_NUMPAD0:
    return "k0";
  case VK_NUMPAD1:
    return "k1";
  case VK_NUMPAD2:
    return "k2";
  case VK_NUMPAD3:
    return "k3";
  case VK_NUMPAD4:
    return "k4";
  case VK_NUMPAD5:
    return "k5";
  case VK_NUMPAD6:
    return "k6";
  case VK_NUMPAD7:
    return "k7";
  case VK_NUMPAD8:
    return "k8";
  case VK_NUMPAD9:
    return "k9";
  case VK_MULTIPLY:
    return "kMultiply";
  case VK_ADD:
    return "kPlus";
  case VK_SEPARATOR:
    return "kComma";
  case VK_SUBTRACT:
    return "kMinus";
  case VK_DECIMAL:
    return "kPoint";
  case VK_DIVIDE:
    return "kDivide";
  case VK_F1:
    return "F1";
  case VK_F2:
    return "F2";
  case VK_F3:
    return "F3";
  case VK_F4:
    return "F4";
  case VK_F5:
    return "F5";
  case VK_F6:
    return "F6";
  case VK_F7:
    return "F7";
  case VK_F8:
    return "F8";
  case VK_F9:
    return "F9";
  case VK_F10:
    return "F10";
  case VK_F11:
    return "F11";
  case VK_F12:
    return "F12";
  case VK_F13:
    return "F13";
  case VK_F14:
    return "F14";
  case VK_F15:
    return "F15";
  case VK_F16:
    return "F16";
  case VK_F17:
    return "F17";
  case VK_F18:
    return "F18";
  case VK_F19:
    return "F19";
  case VK_F20:
    return "F20";
  case VK_F21:
    return "F21";
  case VK_F22:
    return "F22";
  case VK_F23:
    return "F23";
  case VK_F24:
    return "F24";
  }

  return nullptr;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                LPARAM lparam) {
  if (msg == WM_CREATE) {
    auto createStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
    SetWindowLongPtr(hwnd, GWLP_USERDATA,
                     reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
    return 0;
  }

  auto p = GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (p) {
    auto w = reinterpret_cast<Win32Window *>(p);
    return w->Proc(hwnd, msg, wparam, lparam);
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}

Win32Window::~Win32Window() {
  DestroyWindow((HWND)_hwnd);
  UnregisterClass(_class_name.c_str(), (HINSTANCE)_instance);
}

void *Win32Window::Create(void *instance, const wchar_t *class_name,
                          const wchar_t *window_title) {
  _instance = instance;
  _class_name = class_name;

  WNDCLASSEX window_class{.cbSize = sizeof(WNDCLASSEX),
                          .style = CS_HREDRAW | CS_VREDRAW,
                          .lpfnWndProc = ::WndProc,
                          .hInstance = (HINSTANCE)instance,
                          .hIcon = static_cast<HICON>(LoadImage(
                              GetModuleHandle(NULL), L"NVIM_ICON", IMAGE_ICON,
                              LR_DEFAULTSIZE, LR_DEFAULTSIZE, 0)),
                          .hCursor = LoadCursor(NULL, IDC_ARROW),
                          .hbrBackground = nullptr,
                          .lpszClassName = _class_name.c_str(),
                          .hIconSm = static_cast<HICON>(LoadImage(
                              GetModuleHandle(NULL), L"NVIM_ICON", IMAGE_ICON,
                              LR_DEFAULTSIZE, LR_DEFAULTSIZE, 0))};
  if (!RegisterClassEx(&window_class)) {
    return nullptr;
  }

  _hwnd = CreateWindowEx(WS_EX_ACCEPTFILES, _class_name.c_str(), window_title,
                         WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                         CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
                         (HINSTANCE)instance, this);
  return _hwnd;
}

uint64_t Win32Window::Proc(void *hwnd, uint32_t msg, uint64_t wparam,
                           uint64_t lparam) {
  switch (msg) {
  case WM_SIZE: {
    if (wparam != SIZE_MINIMIZED) {
      uint32_t new_width = LOWORD(lparam);
      uint32_t new_height = HIWORD(lparam);
      _on_resize(new_width, new_height);
    }
    return 0;
  }
  case WM_MOVE: {
    // RECT window_rect;
    // DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect,
    //                       sizeof(RECT)); // Get window position without
    //                       shadows
    // HMONITOR monitor = MonitorFromPoint({window_rect.left, window_rect.top},
    //                                     MONITOR_DEFAULTTONEAREST);
    // UINT current_dpi = 0;
    // GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &current_dpi, &current_dpi);

    // if (current_dpi != context->saved_dpi_scaling) {
    //   float dpi_scale = static_cast<float>(current_dpi) /
    //                     static_cast<float>(context->saved_dpi_scaling);
    //   GetWindowRect(hwnd,
    //                 &window_rect); // Window RECT with shadows
    //   int new_window_width =
    //       (window_rect.right - window_rect.left) * dpi_scale + 0.5f;
    //   int new_window_height =
    //       (window_rect.bottom - window_rect.top) * dpi_scale + 0.5f;

    //   // Make sure window is not larger than the actual
    //   // monitor
    //   MONITORINFO monitor_info;
    //   monitor_info.cbSize = sizeof(monitor_info);
    //   GetMonitorInfo(monitor, &monitor_info);
    //   uint32_t monitor_width =
    //       monitor_info.rcWork.right - monitor_info.rcWork.left;
    //   uint32_t monitor_height =
    //       monitor_info.rcWork.bottom - monitor_info.rcWork.top;
    //   if (new_window_width > monitor_width)
    //     new_window_width = monitor_width;
    //   if (new_window_height > monitor_height)
    //     new_window_height = monitor_height;

    //   SetWindowPos(hwnd, nullptr, 0, 0, new_window_width, new_window_height,
    //                SWP_NOMOVE | SWP_NOOWNERZORDER);

    //   auto fontSize = context->renderer->SetDpiScale(current_dpi);
    //   auto size = context->renderer->Size();
    //   auto [rows, cols] = GridSize::FromWindowSize(
    //       size.width, size.height, fontSize.width, fontSize.height);
    //   if (context->_grid.Rows() != rows || context->_grid.Cols() != rows) {
    //     context->SendResize(rows, cols);
    //   }
    //   context->saved_dpi_scaling = current_dpi;
    // }
    return 0;
  }
  case WM_DESTROY: {
    PostQuitMessage(0);
    return 0;
  }
    // case WM_RENDERER_FONT_UPDATE: {
    //   auto size = context->renderer->Size();
    //   auto fontSize = context->renderer->FontSize();
    //   auto [rows, cols] = GridSize::FromWindowSize(
    //       size.width, size.height, fontSize.width, fontSize.height);
    //   context->SendResize(rows, cols);
    //   return 0;
    // }

  case WM_DEADCHAR:
  case WM_SYSDEADCHAR: {
    _dead_char_pending = true;
    return 0;
  }

  case WM_CHAR: {
    _dead_char_pending = false;
    // Special case for <LT>
    if (wparam == 0x3C) {
      _on_input("<LT>");
    } else {
      _on_char(static_cast<wchar_t>(wparam));
    }
    return 0;
  }

  case WM_SYSCHAR: {
    _dead_char_pending = false;
    _on_sys_char(static_cast<wchar_t>(wparam));
    return 0;
  }

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    // Special case for <ALT+ENTER> (fullscreen transition)
    if (((GetKeyState(VK_MENU) & 0x80) != 0) && wparam == VK_RETURN) {
      _on_toggle_screen();
    } else {
      LONG msg_pos = GetMessagePos();
      POINTS pt = MAKEPOINTS(msg_pos);
      MSG current_msg{.hwnd = (HWND)hwnd,
                      .message = msg,
                      .wParam = wparam,
                      .lParam = (LPARAM)lparam,
                      .time = static_cast<DWORD>(GetMessageTime()),
                      .pt = POINT{pt.x, pt.y}};

      if (_dead_char_pending) {
        if (static_cast<int>(wparam) == VK_SPACE ||
            static_cast<int>(wparam) == VK_BACK ||
            static_cast<int>(wparam) == VK_ESCAPE) {
          _dead_char_pending = false;
          TranslateMessage(&current_msg);
          return 0;
        }
      }

      // If none of the special keys were hit, process in
      // WM_CHAR
      auto key = GetProcessKey(static_cast<int>(wparam));
      if (key) {
        _on_modified_input(key);
      } else {
        TranslateMessage(&current_msg);
      }
    }
    return 0;
  }

  case WM_MOUSEMOVE: {
    POINTS cursor_pos = MAKEPOINTS(lparam);
    switch (wparam) {
    case MK_LBUTTON:
      _on_mouse_left_drag(cursor_pos.x, cursor_pos.y);
      break;
    case MK_MBUTTON:
      _on_mouse_middle_drag(cursor_pos.x, cursor_pos.y);
      break;
    case MK_RBUTTON:
      _on_mouse_right_drag(cursor_pos.x, cursor_pos.y);
      break;
    }
    return 0;
  }

  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MBUTTONUP: {
    POINTS cursor_pos = MAKEPOINTS(lparam);
    if (msg == WM_LBUTTONDOWN) {
      _on_mouse_left_down(cursor_pos.x, cursor_pos.y);
    } else if (msg == WM_MBUTTONDOWN) {
      _on_mouse_middle_down(cursor_pos.x, cursor_pos.y);
    } else if (msg == WM_RBUTTONDOWN) {
      _on_mouse_right_down(cursor_pos.x, cursor_pos.y);
    } else if (msg == WM_LBUTTONUP) {
      _on_mouse_left_release(cursor_pos.x, cursor_pos.y);
    } else if (msg == WM_MBUTTONUP) {
      _on_mouse_middle_release(cursor_pos.x, cursor_pos.y);
    } else if (msg == WM_RBUTTONUP) {
      _on_mouse_right_release(cursor_pos.x, cursor_pos.y);
    }
    return 0;
  }

  // case WM_XBUTTONDOWN: {
  //   int button = GET_XBUTTON_WPARAM(wparam);
  //   if (button == XBUTTON1 && !context->xbuttons[0]) {
  //     context->SendInput("<C-o>");
  //     context->xbuttons[0] = true;
  //   } else if (button == XBUTTON2 && !context->xbuttons[1]) {
  //     context->SendInput("<C-i>");
  //     context->xbuttons[1] = true;
  //   }
  //   return 0;
  // }

  // case WM_XBUTTONUP: {
  //   int button = GET_XBUTTON_WPARAM(wparam);
  //   if (button == XBUTTON1) {
  //     context->xbuttons[0] = false;
  //   } else if (button == XBUTTON2) {
  //     context->xbuttons[1] = false;
  //   }
  //   return 0;
  // }

    // case WM_MOUSEWHEEL:
    // {
    //     bool should_resize_font = (GetKeyState(VK_CONTROL) & 0x80) !=
    //     0;

    //     POINTS screen_point = MAKEPOINTS(lparam);
    //     POINT client_point{
    //         .x = static_cast<LONG>(screen_point.x),
    //         .y = static_cast<LONG>(screen_point.y),
    //     };
    //     ScreenToClient(hwnd, &client_point);

    //     short wheel_distance = GET_WHEEL_DELTA_WPARAM(wparam);
    //     short scroll_amount = wheel_distance / WHEEL_DELTA;
    //     auto font_size = context->renderer->FontSize();
    //     auto [row, col] =
    //         GridPoint::FromCursor(client_point.x, client_point.y,
    //                               font_size.width, font_size.height);
    //     MouseAction action = scroll_amount > 0
    //                              ? MouseAction::MouseWheelUp
    //                              : MouseAction::MouseWheelDown;

    //     if (should_resize_font)
    //     {
    //         auto fontSize =
    //             context->renderer->ResizeFont(scroll_amount * 2.0f);
    //         auto size = context->renderer->Size();
    //         auto [rows, cols] = GridSize::FromWindowSize(
    //             size.width, size.height, fontSize.width,
    //             fontSize.height);
    //         if (context->_grid.Rows() != rows ||
    //             context->_grid.Cols() != rows)
    //         {
    //             context->SendResize(rows, cols);
    //         }
    //     }
    //     else
    //     {
    //         for (int i = 0; i < abs(scroll_amount); ++i)
    //         {
    //             context->SendMouseInput(MouseButton::Wheel, action,
    //             row,
    //                                     col);
    //         }
    //     }
    // }
    //     return 0;

    // case WM_DROPFILES:
    // {
    //     wchar_t file_to_open[MAX_PATH];
    //     uint32_t num_files =
    //         DragQueryFileW(reinterpret_cast<HDROP>(wparam),
    //         0xFFFFFFFF,
    //                        file_to_open, MAX_PATH);
    //     for (int i = 0; i < num_files; ++i)
    //     {
    //         DragQueryFileW(reinterpret_cast<HDROP>(wparam), i,
    //         file_to_open,
    //                        MAX_PATH);
    //         context->OpenFile(file_to_open);
    //     }
    // }
    //     return 0;
  }

  return DefWindowProc((HWND)hwnd, msg, wparam, lparam);
}
