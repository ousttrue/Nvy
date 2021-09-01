#pragma once
#include "nvim_input.h"
#include <functional>
#include <string>

namespace msgpackpp {
class parser;
}
using on_redraw_t = std::function<void(const msgpackpp::parser &)>;
using on_terminated_t = std::function<void()>;
class NvimFrontend {
  class NvimFrontendImpl *_impl = nullptr;

public:
  NvimFrontend();
  ~NvimFrontend();
  // nvim --embed
  bool Launch(const wchar_t *command, const on_terminated_t &callback);
  // return guifont
  std::string Initialize();

  void AttachUI(const on_redraw_t &callback, int rows, int cols);
  void ResizeGrid(int rows, int cols);

  void Process();
  void Input(const InputEvent &e);
  void Mouse(const MouseEvent &e);
};
