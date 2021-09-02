#include "win32window.h"
#include <Windows.h>
//
#include <dwmapi.h>
#include <nvim_vk_map.h>
#include <shellscalingapi.h>

WINDOWPLACEMENT saved_window_placement = {.length = sizeof(WINDOWPLACEMENT)};

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

  case WM_DESTROY: {
    PostQuitMessage(0);
    return 0;
  }

  case WM_DEADCHAR:
  case WM_SYSDEADCHAR: {
    _dead_char_pending = true;
    return 0;
  }

  case WM_CHAR: {
    _dead_char_pending = false;
    // Special case for <LT>
    if (wparam == 0x3C) {
      _on_input(InputEvent::create_input("<LT>"));
    } else {
      _on_input(InputEvent::create_char(wparam));
    }
    return 0;
  }

  case WM_SYSCHAR: {
    _dead_char_pending = false;
    _on_input(InputEvent::create_syschar(wparam));
    return 0;
  }

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    // Special case for <ALT+ENTER> (fullscreen transition)
    if (((GetKeyState(VK_MENU) & 0x80) != 0) && wparam == VK_RETURN) {
      ToggleFullscreen();
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
      auto key =
          Nvim_VK_Map(static_cast<int>(wparam), GetKeyState(VK_CONTROL) < 0);
      if (key) {
        _on_input(InputEvent::create_modified(key));
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
      _on_mouse(
          {cursor_pos.x, cursor_pos.y, MouseButton::Left, MouseAction::Drag});
      break;
    case MK_MBUTTON:
      _on_mouse(
          {cursor_pos.x, cursor_pos.y, MouseButton::Middle, MouseAction::Drag});
      break;
    case MK_RBUTTON:
      _on_mouse(
          {cursor_pos.x, cursor_pos.y, MouseButton::Right, MouseAction::Drag});
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
      _on_mouse(
          {cursor_pos.x, cursor_pos.y, MouseButton::Left, MouseAction::Press});
    } else if (msg == WM_MBUTTONDOWN) {
      _on_mouse({cursor_pos.x, cursor_pos.y, MouseButton::Middle,
                 MouseAction::Press});
    } else if (msg == WM_RBUTTONDOWN) {
      _on_mouse(
          {cursor_pos.x, cursor_pos.y, MouseButton::Right, MouseAction::Press});
    } else if (msg == WM_LBUTTONUP) {
      _on_mouse({cursor_pos.x, cursor_pos.y, MouseButton::Left,
                 MouseAction::Release});
    } else if (msg == WM_MBUTTONUP) {
      _on_mouse({cursor_pos.x, cursor_pos.y, MouseButton::Middle,
                 MouseAction::Release});
    } else if (msg == WM_RBUTTONUP) {
      _on_mouse({cursor_pos.x, cursor_pos.y, MouseButton::Right,
                 MouseAction::Release});
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

bool Win32Window::Loop() {
  MSG msg;
  uint32_t previous_width = 0, previous_height = 0;

  // windows msg
  while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {

    if (!GetMessage(&msg, NULL, 0, 0)) {
      return false;
    }

    // TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return true;
}

void Win32Window::ToggleFullscreen() {
  auto hwnd = (HWND)_hwnd;
  DWORD style = GetWindowLong(hwnd, GWL_STYLE);
  MONITORINFO mi{.cbSize = sizeof(MONITORINFO)};
  if (style & WS_OVERLAPPEDWINDOW) {
    if (GetWindowPlacement(hwnd, &saved_window_placement) &&
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                       &mi)) {
      SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
      SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                   mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
  } else {
    SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(hwnd, &saved_window_placement);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                     SWP_FRAMECHANGED);
  }
}

void Win32Window::Resize(int w, int h) {
  RECT client_rect;
  auto hwnd = (HWND)_hwnd;
  GetClientRect(hwnd, &client_rect);
  MoveWindow(hwnd, client_rect.left, client_rect.top, w, h, false);
}

std::tuple<int, int> Win32Window::Size() const {
  RECT client_rect;
  auto hwnd = (HWND)_hwnd;
  GetClientRect(hwnd, &client_rect);
  return {client_rect.right - client_rect.left,
          client_rect.bottom - client_rect.top};
}

uint32_t Win32Window::GetMonitorDpi() const {
  RECT window_rect;
  DwmGetWindowAttribute((HWND)_hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect,
                        sizeof(RECT));
  HMONITOR monitor = MonitorFromPoint({window_rect.left, window_rect.top},
                                      MONITOR_DEFAULTTONEAREST);

  UINT dpi = 0;
  GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi, &dpi);
  return dpi;
}
