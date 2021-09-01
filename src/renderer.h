#pragma once
#include <nvim_renderer.h>

struct HighlightAttribute;
class NvimGrid;
class Renderer : public NvimRenderer {
  class RendererImpl *_impl = nullptr;

public:
  Renderer(bool disable_ligatures, float linespace_factor, uint32_t monitor_dpi,
           const HighlightAttribute *defaultHL);
  ~Renderer();
  // font size
  void SetFont(std::string_view font, float size) override;
  std::tuple<float, float> FontSize() const override;
  // render
  std::tuple<int, int> StartDraw(struct ID3D11Device2 *device,
                                 struct IDXGISurface2 *backbuffer) override;
  void FinishDraw() override;
  void DrawBackgroundRect(int rows, int cols,
                          const HighlightAttribute *hl) override;
  void DrawGridLine(const NvimGrid *grid, int row) override;
  void DrawCursor(const NvimGrid *grid) override;
  void DrawBorderRectangles(const NvimGrid *grid, int width,
                            int height) override;
};
