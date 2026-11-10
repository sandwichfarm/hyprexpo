#include "WindowIcons.hpp"
#include "IconIndex.hpp"

#include <hyprgraphics/image/Image.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/desktop/view/window/WindowMetadata.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include <fstream>
#include <memory>
#include <unordered_map>

namespace {
std::unique_ptr<Hyprexpo::CIconIndex>                 g_index; // built on first use, dropped on config reload
std::unordered_map<pid_t, std::string>                g_processNames;
std::unordered_map<std::string, SP<Render::ITexture>> g_textures; // icon path + size; null = failed to load

const Hyprexpo::CIconIndex& iconIndex() {
    if (!g_index)
        g_index = std::make_unique<Hyprexpo::CIconIndex>(Hyprexpo::defaultIconSources());
    return *g_index;
}

const std::string& processName(pid_t pid) {
    auto [it, inserted] = g_processNames.try_emplace(pid);
    if (inserted) {
        std::ifstream file("/proc/" + std::to_string(pid) + "/comm");
        std::getline(file, it->second);
    }
    return it->second;
}

SP<Render::ITexture> loadTexture(const std::filesystem::path& path, int sizePx) {
    Hyprgraphics::CImage image(path.string(), Vector2D{(double)sizePx, (double)sizePx});
    if (!image.success() || !image.cairoSurface() || !image.cairoSurface()->cairo())
        return nullptr;
    return g_pHyprRenderer->createTexture(image.cairoSurface()->cairo());
}
}

SP<Render::ITexture> windowIconTexture(const PHLWINDOW& window, int sizePx) {
    // Windows with a custom class and no .desktop entry (e.g. `foot --app-id=...`) fall back to their program.
    auto path = iconIndex().iconFor(window->metadata().appID());
    if (!path)
        path = iconIndex().iconFor(processName(window->backend().pid()));
    if (!path)
        return nullptr;

    const auto KEY = path->string() + "\n" + std::to_string(sizePx);
    if (const auto IT = g_textures.find(KEY); IT != g_textures.end())
        return IT->second;
    return g_textures[KEY] = loadTexture(*path, sizePx);
}

void clearWindowIconCache() {
    g_index.reset();
    g_processNames.clear();
    g_textures.clear();
}
