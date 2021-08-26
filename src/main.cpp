#include "grid.h"
#include "nvim_frontend.h"
#include "renderer.h"
#include "vec.h"
#include "win32window.h"
#include "window_messages.h"
#include <Windows.h>
#include <msgpackpp/msgpackpp.h>
#include <msgpackpp/rpc.h>
#include <msgpackpp/windows_pipe_transport.h>
#include <plog/Appenders/DebugOutputAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>
#include <shellapi.h>

const int MAX_NVIM_CMD_LINE_SIZE = 32767;
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR p_cmd_line, int n_cmd_show) {
  static plog::DebugOutputAppender<plog::TxtFormatter> debugOutputAppender;
  plog::init(plog::verbose, &debugOutputAppender);

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  auto cmd = CommandLine::Get();

  Win32Window window;
  auto hwnd = (HWND)window.Create(instance, L"Nvy_Class", L"Nvy");
  if (!hwnd) {
    return 1;
  }

  Grid grid;
  // context._grid.OnSizeChanged([&context](const GridSize &size) {
  //   context.SendResize(size.rows, size.cols);
  // });

  Renderer renderer(hwnd, cmd.disable_ligatures, cmd.linespace_factor,
                    &grid.hl(0));
  window._on_resize = [&renderer](int w, int h) { renderer.Resize(w, h); };

  NvimFrontend nvim;
  if (!nvim.Launch(cmd.nvim_command_line)) {
    return 3;
  }

  // setfont
  auto guifont_buffer = nvim.Initialize();
  if (!guifont_buffer.empty()) {
    renderer.UpdateGuiFont(guifont_buffer.data(), guifont_buffer.size());
  }

  // initial window size
  if (cmd.start_maximized) {
    window.ToggleFullscreen();
  } else if (cmd.rows != 0 && cmd.cols != 0) {
    PixelSize start_size = renderer.GridToPixelSize(cmd.rows, cmd.cols);
    window.Resize(start_size.width, start_size.height);
  }
  ShowWindow(hwnd, SW_SHOWDEFAULT);

  // Attach the renderer now that the window size is
  // determined
  renderer.Attach();
  // auto size = renderer.Size();
  // auto fontSize = renderer.FontSize();
  // auto gridSize = GridSize::FromWindowSize(size.width, size.height,
  //                                          fontSize.width, fontSize.height);


  // window._on_input = [&context](auto input) {
  //   context.SendInput(input.data());
  // };
  // window._on_char = [&context](auto ch) { context.SendChar(ch); };
  // window._on_sys_char = [&context](auto ch) { context.SendSysChar(ch); };
  // window._on_toggle_screen = [&context]() { context.ToggleFullscreen(); };
  // window._on_modified_input = [&context](auto input) {
  //   context.NvimSendModifiedInput(input.data(), true);
  // };
  // // mouse drag
  // window._on_mouse_left_drag = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto grid_pos =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   if (context.cached_cursor_grid_pos.col != grid_pos.col ||
  //       context.cached_cursor_grid_pos.row != grid_pos.row) {
  //     context.SendMouseInput(MouseButton::Left, MouseAction::Drag,
  //     grid_pos.row,
  //                            grid_pos.col);
  //     context.cached_cursor_grid_pos = grid_pos;
  //   }
  // };
  // window._on_mouse_middle_drag = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto grid_pos =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   if (context.cached_cursor_grid_pos.col != grid_pos.col ||
  //       context.cached_cursor_grid_pos.row != grid_pos.row) {
  //     context.SendMouseInput(MouseButton::Middle, MouseAction::Drag,
  //                            grid_pos.row, grid_pos.col);
  //     context.cached_cursor_grid_pos = grid_pos;
  //   }
  // };
  // window._on_mouse_right_drag = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto grid_pos =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   if (context.cached_cursor_grid_pos.col != grid_pos.col ||
  //       context.cached_cursor_grid_pos.row != grid_pos.row) {
  //     context.SendMouseInput(MouseButton::Right, MouseAction::Drag,
  //                            grid_pos.row, grid_pos.col);
  //     context.cached_cursor_grid_pos = grid_pos;
  //   }
  // };
  // // mouse button
  // window._on_mouse_left_down = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Left, MouseAction::Press, row, col);
  // };
  // window._on_mouse_middle_down = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Middle, MouseAction::Press, row,
  //   col);
  // };
  // window._on_mouse_right_down = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Right, MouseAction::Press, row, col);
  // };
  // window._on_mouse_left_release = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Left, MouseAction::Release, row,
  //   col);
  // };
  // window._on_mouse_middle_release = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Middle, MouseAction::Release, row,
  //   col);
  // };
  // window._on_mouse_right_release = [&context](int x, int y) {
  //   auto fontSize = renderer.FontSize();
  //   auto [row, col] =
  //       GridPoint::FromCursor(x, y, fontSize.width, fontSize.height);
  //   context.SendMouseInput(MouseButton::Right, MouseAction::Release, row,
  //   col);
  // };

  while (window.Loop()) {
    nvim.Process();
  }

  return 0;
}
