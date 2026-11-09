#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Texture.hpp>

// App icon for a window, looked up through its .desktop entry and the icon theme.
// Results (including misses) are cached per window class and size; null when no icon is found.
SP<Render::ITexture> windowIconTexture(const PHLWINDOW& window, int sizePx);
void                 clearWindowIconCache();
