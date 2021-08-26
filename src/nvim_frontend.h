#pragma once
#include "windowevent.h"
#include <string>

class NvimFrontend {
  class NvimFrontendImpl *_impl = nullptr;

public:
  NvimFrontend();
  ~NvimFrontend();
  // nvim --embed
  bool Launch(const wchar_t *command);
  // return guifont
  std::string Initialize();

  void Process();
  void Input(const InputEvent &e);
  void Mouse(const MouseEvent &e);
};
