#pragma once
#include <functional>
using on_terminated_t = std::function<void()>;

class NvimPipe {
public:
  NvimPipe();
  ~NvimPipe();
  NvimPipe(const NvimPipe &) = delete;
  NvimPipe &operator=(const NvimPipe &) = delete;
  bool Launch(const wchar_t *command_line, const on_terminated_t &callback);
  void *ReadHandle();
  void *WriteHandle();
};
