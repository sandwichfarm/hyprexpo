#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Texture.hpp>

// App icon for a window, looked up through its .desktop entry and the icon theme; null when none is found.
// The lookup index and textures are cached until clearWindowIconCache() (config reload, plugin exit).
SP<Render::ITexture> windowIconTexture(const PHLWINDOW& window, int sizePx);
void                 clearWindowIconCache();
