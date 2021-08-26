#pragma once

class NvimPipe {
public:
  NvimPipe();
  ~NvimPipe();
  NvimPipe(const NvimPipe &) = delete;
  NvimPipe &operator=(const NvimPipe &) = delete;
  bool Launch(const wchar_t *command_line);
  void *ReadHandle();
  void *WriteHandle();
};
