#pragma once
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
};
