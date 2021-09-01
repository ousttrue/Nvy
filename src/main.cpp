#include "nvim_frontend.h"
#include "nvim_grid.h"
#include "nvim_redraw.h"
#include "renderer.h"
#include "renderer/d3d.h"
#include "renderer/swapchain.h"
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

  NvimGrid grid;
  Renderer renderer(cmd.disable_ligatures, cmd.linespace_factor,
                    window.GetMonitorDpi(), &grid.hl(0));

  NvimFrontend nvim;
  if (!nvim.Launch(cmd.nvim_command_line,
                   [hwnd]() { PostMessage(hwnd, WM_CLOSE, 0, 0); })) {
    return 3;
  }

  // setfont
  auto guifont = nvim.Initialize();
  // Consolas:h14
  auto [font, size] = NvimRedraw::ParseGUIFont(guifont);
  renderer.SetFont(font, size);

  // initial window size
  if (cmd.start_maximized) {
    window.ToggleFullscreen();
  } else if (cmd.rows != 0 && cmd.cols != 0) {
    auto [font_width, font_height] = renderer.FontSize();
    auto requested_width = static_cast<int>(ceilf(font_width) * cmd.cols);
    auto requested_height = static_cast<int>(ceilf(font_height) * cmd.rows);

    // Adjust size to include title bar
    RECT rect = {0, 0, requested_width, requested_height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
    window.Resize(rect.right - rect.left, rect.bottom - rect.top);
  }
  UpdateWindow(hwnd);
  ShowWindow(hwnd, SW_SHOWDEFAULT);

  auto d3d = D3D::Create();
  auto swapchain = Swapchain::Create(d3d->Device(), hwnd);

  window._on_input = [&nvim](const InputEvent &input) { nvim.Input(input); };
  window._on_mouse = [&nvim, &renderer](const MouseEvent &mouse) {
    auto [font_width, font_height] = renderer.FontSize();
    auto grid_pos = GridPoint::FromCursor(mouse.x, mouse.y, ceilf(font_width),
                                          ceilf(font_height));
    auto copy = mouse;
    copy.x = grid_pos.col;
    copy.y = grid_pos.row;
    nvim.Mouse(copy);
  };

  // Attach the renderer now that the window size is determined
  auto [window_width, window_height] = window.Size();
  auto [font_width, font_height] = renderer.FontSize();
  auto gridSize = GridSize::FromWindowSize(
      window_width, window_height, ceilf(font_width), ceilf(font_height));

  NvimRedraw redraw;
  nvim.AttachUI(
      [&redraw, &renderer, &grid, &window, &d3d, &swapchain,
       &nvim](const msgpackpp::parser &msg) {
        auto [window_width, window_height] = window.Size();

        // if (this->_swapchain) {
        uint32_t w, h;
        std::tie(w, h) = swapchain->GetSize();
        if (window_width != w && window_height != h) {

          HRESULT hr = swapchain->Resize(window_width, window_height);
          if (hr == DXGI_ERROR_DEVICE_REMOVED) {
            assert(false);
          }
          // } else {
          //   this->_swapchain = Swapchain::Create(_d3d->Device(), _hwnd);
          // }
        }

        auto dxgi_backbuffer = swapchain->GetBackbuffer();

        redraw.Dispatch(d3d->Device().Get(), dxgi_backbuffer.Get(), &grid,
                        &renderer, msg);

        auto hr = swapchain->PresentCopyFrontToBack(d3d->Context());

        // clear render target
        ID3D11RenderTargetView *null_views[] = {nullptr};
        d3d->Context()->OMSetRenderTargets(ARRAYSIZE(null_views), null_views,
                                           nullptr);
        d3d->Context()->Flush();

        if (hr == DXGI_ERROR_DEVICE_REMOVED) {
          assert(false);
          // this->HandleDeviceLost();
        }
      },
      gridSize.rows, gridSize.cols);

  while (window.Loop()) {
    auto [window_width, window_height] = window.Size();
    auto [font_width, font_height] = renderer.FontSize();
    auto gridSize = GridSize::FromWindowSize(
        window_width, window_height, ceilf(font_width), ceilf(font_height));
    if (redraw.Sizing()) {
      auto a = 0;
    } else {
      if (grid.Rows() != gridSize.rows || grid.Cols() != gridSize.cols) {
        redraw.SetSizing();
        nvim.ResizeGrid(gridSize.rows, gridSize.cols);
      }
    }

    nvim.Process();
  }

  return 0;
}
