#include "grid.h"
#include "nvim_frontend.h"
#include "nvim_redraw.h"
#include "renderer.h"
#include "win32window.h"
#include <Windows.h>
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
  Renderer renderer(hwnd, cmd.disable_ligatures, cmd.linespace_factor,
                    &grid.hl(0));
  // window._on_resize = [&renderer](int w, int h) {
  //   PLOGD << "window: [" << w << ", " << h << "]";
  //   renderer.Resize(w, h);
  // };

  NvimFrontend nvim;
  if (!nvim.Launch(cmd.nvim_command_line,
                   [hwnd]() { PostMessage(hwnd, WM_CLOSE, 0, 0); })) {
    return 3;
  }
  grid.OnSizeChanged(
      [&nvim](const GridSize &size) { nvim.ResizeGrid(size.rows, size.cols); });
  renderer.OnRowsCols([&grid](int rows, int cols) {
    PLOGD << "renderer: [" << cols << ", " << rows << "]";
    grid.RowsCols(rows, cols);
  });

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
  UpdateWindow(hwnd);
  ShowWindow(hwnd, SW_SHOWDEFAULT);

  window._on_input = [&nvim](const InputEvent &input) { nvim.Input(input); };
  window._on_mouse = [&nvim, &renderer](const MouseEvent &mouse) {
    auto fontSize = renderer.FontSize();
    auto grid_pos = GridPoint::FromCursor(mouse.x, mouse.y, fontSize.width,
                                          fontSize.height);
    auto copy = mouse;
    copy.x = grid_pos.col;
    copy.y = grid_pos.row;
    nvim.Mouse(copy);
  };

  // Attach the renderer now that the window size is
  // determined
  auto [w, h] = window.Size();
  renderer.Resize(w, h);
  auto size = renderer.Size();
  auto fontSize = renderer.FontSize();
  auto gridSize = GridSize::FromWindowSize(size.width, size.height,
                                           fontSize.width, fontSize.height);

  NvimRedraw redraw;
  nvim.AttachUI(
      [&redraw, &renderer, &grid](const msgpackpp::parser &msg) {
        redraw.Dispatch(&grid, &renderer, msg);
      },
      gridSize.rows, gridSize.cols);

  while (window.Loop()) {
    auto [w, h] = window.Size();
    renderer.Resize(w, h);
    nvim.Process();
  }

  return 0;
}
