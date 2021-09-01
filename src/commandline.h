#pragma once
#include <stdint.h>
#include <Windows.h>
#include <shellapi.h>
#include <stdlib.h>

constexpr int MAX_NVIM_CMD_LINE_SIZE = 32767;
struct CommandLine {
  bool start_maximized = false;
  bool disable_ligatures = false;
  float linespace_factor = 1.0f;
  int64_t rows = 0;
  int64_t cols = 0;
  wchar_t nvim_command_line[MAX_NVIM_CMD_LINE_SIZE] = {};

  void Parse() {
    int n_args;
    auto cmd_line_args = CommandLineToArgvW(GetCommandLineW(), &n_args);
    int cmd_line_size_left = MAX_NVIM_CMD_LINE_SIZE - wcslen(L"nvim --embed");
    wcscpy_s(nvim_command_line, MAX_NVIM_CMD_LINE_SIZE, L"nvim --embed");

    // Skip argv[0]
    for (int i = 1; i < n_args; ++i) {
      if (!wcscmp(cmd_line_args[i], L"--maximize")) {
        start_maximized = true;
      } else if (!wcscmp(cmd_line_args[i], L"--disable-ligatures")) {
        disable_ligatures = true;
      } else if (!wcsncmp(cmd_line_args[i], L"--geometry=",
                          wcslen(L"--geometry="))) {
        wchar_t *end_ptr;
        cols = wcstol(&cmd_line_args[i][11], &end_ptr, 10);
        rows = wcstol(end_ptr + 1, nullptr, 10);
      } else if (!wcsncmp(cmd_line_args[i], L"--linespace-factor=",
                          wcslen(L"--linespace-factor="))) {
        wchar_t *end_ptr;
        float factor = wcstof(&cmd_line_args[i][19], &end_ptr);
        if (factor > 0.0f && factor < 20.0f) {
          linespace_factor = factor;
        }
      }
      // Otherwise assume the argument is a filename to open
      else {
        size_t arg_size = wcslen(cmd_line_args[i]);
        if (arg_size <= (cmd_line_size_left + 3)) {
          wcscat_s(nvim_command_line, cmd_line_size_left, L" \"");
          cmd_line_size_left -= 2;
          wcscat_s(nvim_command_line, cmd_line_size_left, cmd_line_args[i]);
          cmd_line_size_left -= arg_size;
          wcscat_s(nvim_command_line, cmd_line_size_left, L"\"");
          cmd_line_size_left -= 1;
        }
      }
    }
  }

  static CommandLine Get() {
    CommandLine cmd;
    cmd.Parse();
    return cmd;
  }
};
