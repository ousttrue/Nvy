#pragma once
#include <stdint.h>

enum class InputEventTypes {
  Input,
  Char,
  SysChar,
  ModifiedInput,
};

struct InputEvent {

  InputEventTypes type;
  union {
    wchar_t ch;
    const char *input;
  };

  static InputEvent create_input(const char *input) {
    InputEvent e;
    e.type = InputEventTypes::Input;
    e.input = input;
    return e;
  }

  static InputEvent create_modified(const char *input) {
    InputEvent e;
    e.type = InputEventTypes::ModifiedInput;
    e.input = input;
    return e;
  }

  static InputEvent create_char(uint64_t wparam) {
    InputEvent e;
    e.type = InputEventTypes::Char;
    e.ch = static_cast<wchar_t>(wparam);
    return e;
  }

  static InputEvent create_syschar(uint64_t wparam) {
    InputEvent e;
    e.type = InputEventTypes::SysChar;
    e.ch = static_cast<wchar_t>(wparam);
    return e;
  }
};

enum class MouseButton { Left, Right, Middle, Wheel };
enum class MouseAction {
  Press,
  Drag,
  Release,
  MouseWheelUp,
  MouseWheelDown,
  MouseWheelLeft,
  MouseWheelRight
};

struct MouseEvent {
  int x;
  int y;
  MouseButton button;
  MouseAction action;
};

constexpr const char *GetMouseBotton(MouseButton button) {
  switch (button) {
  case MouseButton::Left:
    return "left";
  case MouseButton::Right:
    return "right";
  case MouseButton::Middle:
    return "middle";
  case MouseButton::Wheel:
    return "wheel";
  default:
    // assert(false);
    return nullptr;
  }
}

constexpr const char *GetMouseAction(MouseAction action) {
  switch (action) {
  case MouseAction::Press:
    return "press";
  case MouseAction::Drag:
    return "drag";
  case MouseAction::Release:
    return "release";
  case MouseAction::MouseWheelUp:
    return "up";
  case MouseAction::MouseWheelDown:
    return "down";
  case MouseAction::MouseWheelLeft:
    return "left";
  case MouseAction::MouseWheelRight:
    return "right";
  default:
    // assert(false);
    return nullptr;
  }
}
