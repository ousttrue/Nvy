#pragma once
#include "grid_size.h"
#include "nvim_input.h"
#include <functional>
#include <string>

namespace msgpackpp {
class parser;
}
using on_terminated_t = std::function<void()>;
class NvimFrontend {
  class NvimFrontendImpl *_impl = nullptr;

public:
  NvimFrontend();
  ~NvimFrontend();
  // nvim --embed
  bool Launch(const wchar_t *command, const on_terminated_t &callback);
  // return guifont
  std::tuple<std::string_view, float> Initialize();

  void AttachUI(class NvimRenderer *renderer, int rows, int cols);
  void ResizeGrid(int rows, int cols);

  void Process();
  void Input(const InputEvent &e);
  void Mouse(const MouseEvent &e);

  GridSize GridSize() const;
  bool Sizing() const;
  void SetSizing();

  const struct HighlightAttribute *DefaultAttribute() const;
};
