#pragma once
#include "windowevent.h"
#include <functional>
#include <string>

namespace msgpackpp {
class parser;
}
using on_redraw_t =
    std::function<void(const msgpackpp::parser &)>;

class NvimFrontend {
  class NvimFrontendImpl *_impl = nullptr;

public:
  NvimFrontend();
  ~NvimFrontend();
  // nvim --embed
  bool Launch(const wchar_t *command);
  // return guifont
  std::string Initialize();

  void OnRedraw(const on_redraw_t &callback);

  void Process();
  void Input(const InputEvent &e);
  void Mouse(const MouseEvent &e);
};
