#include "commandline.h"
#include "renderer/d3d.h"
#include "renderer/swapchain.h"
#include "win32window.h"
#include <Windows.h>
#include <nvim_frontend.h>
#include <nvim_renderer_d2d.h>
#include <plog/Appenders/DebugOutputAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    PWSTR p_cmd_line, int n_cmd_show) {
  static plog::DebugOutputAppender<plog::TxtFormatter> debugOutputAppender;
  plog::init(plog::verbose, &debugOutputAppender);

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  // parse commandline
  auto cmd = CommandLine::Get();

  // create window
  Win32Window window;
  auto hwnd = (HWND)window.Create(instance, L"Nvy_Class", L"Nvy");
  if (!hwnd) {
    return 1;
  }

  // launch nvim
  NvimFrontend nvim;
  if (!nvim.Launch(cmd.nvim_command_line,
                   [hwnd]() { PostMessage(hwnd, WM_CLOSE, 0, 0); })) {
    return 3;
  }
  auto [font, size] = nvim.Initialize();

  // setup renderer
  // create swapchain
  auto d3d = D3D::Create();
  auto swapchain = Swapchain::Create(d3d->Device(), hwnd);

  NvimRendererD2D renderer(d3d->Device().Get(), nvim.DefaultAttribute(),
                           cmd.disable_ligatures, cmd.linespace_factor,
                           window.GetMonitorDpi());
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

  // bind window event
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
  window._on_drop_file = [&nvim](const wchar_t *file) {
    nvim.OpenFile(file);
  };

  // Attach the renderer now that the window size is determined
  auto [window_width, window_height] = window.Size();
  auto [font_width, font_height] = renderer.FontSize();
  auto gridSize = GridSize::FromWindowSize(
      window_width, window_height, ceilf(font_width), ceilf(font_height));

  // nvim_attach_ui. start redraw message
  nvim.AttachUI(&renderer, gridSize.rows, gridSize.cols);

  // main loop
  while (window.Loop()) {
    auto [window_width, window_height] = window.Size();

    // update swapchain size
    auto [w, h] = swapchain->GetSize();
    if (window_width != w || window_height != h) {
      HRESULT hr = swapchain->Resize(window_width, window_height);
      if (hr == DXGI_ERROR_DEVICE_REMOVED) {
        assert(false);
      }
    }

    // update nvim gird size
    auto [font_width, font_height] = renderer.FontSize();
    auto gridSize = GridSize::FromWindowSize(
        window_width, window_height, ceilf(font_width), ceilf(font_height));
    if (nvim.Sizing()) {
      auto a = 0;
    } else {
      if (nvim.GridSize() != gridSize) {
        nvim.SetSizing();
        nvim.ResizeGrid(gridSize.rows, gridSize.cols);
      }
    }

    {
      // parepare render target
      renderer.SetTarget(swapchain->GetBackbuffer().Get());
      // process nvim message. may render
      nvim.Process();
      // release backbuffer reference
      renderer.SetTarget(nullptr);

      // present
      auto hr = swapchain->PresentCopyFrontToBack(d3d->Context());
      if (hr == DXGI_ERROR_DEVICE_REMOVED) {
        assert(false);
        // this->HandleDeviceLost();
      }
    }
  }

  return 0;
}
