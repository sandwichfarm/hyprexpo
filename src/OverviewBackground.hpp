#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Texture.hpp>

namespace Render { class CRenderContext; }

// Release cached textures on config reload and before plugin unload.
void clearOverviewBackgroundCache();

class COverviewBackground {
  public:
    void invalidate() { m_dirty = true; }
    void update(const PHLMONITOR& monitor);
    bool render(Render::CRenderContext& ctx, const PHLMONITOR& monitor, float alpha = 1.F) const;

  private:
    bool m_dirty = true;
    Vector2D m_size;
    SP<Render::ITexture> m_texture;
};
