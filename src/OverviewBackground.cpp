#include "OverviewBackground.hpp"
#include "BackgroundImageLogic.hpp"
#include "HyprlandConfigCompat.hpp"
#include "globals.hpp"

#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprgraphics/image/Image.hpp>
#include <cstdlib>
#include <list>
#include <memory>
#include <string>

namespace {

struct SCachedBackground {
    std::string path;
    std::string fit;
    int radius;
    Vector2D size;
    SP<Render::ITexture> texture;
};

// Keep at most four output-sized textures, shared by sessions/monitors that
// use the same image and dimensions. Failed loads are cached until reload too.
std::list<SCachedBackground> backgroundCache;

}

void clearOverviewBackgroundCache() {
    Render::GL::g_pHyprOpenGL->makeEGLCurrent();
    backgroundCache.clear();
}

void COverviewBackground::update(const PHLMONITOR& monitor) {
    if (!monitor || (!m_dirty && m_size == monitor->m_transformedSize))
        return;
    m_dirty = false;
    m_size = monitor->m_transformedSize;
    Render::GL::g_pHyprOpenGL->makeEGLCurrent();
    m_texture.reset();

    std::string path = CompatHyprlandAPI::stringValue("plugin:hyprexpo:bg_image");
    if (path.empty())
        return;
    if (path.starts_with("~/")) {
        if (const auto* home = std::getenv("HOME"))
            path.replace(0, 1, home);
    }
    const auto fit = CompatHyprlandAPI::stringValue("plugin:hyprexpo:bg_image_fit");
    const int radius = static_cast<int>(std::clamp<Config::INTEGER>(CompatHyprlandAPI::intValue("plugin:hyprexpo:bg_image_blur"), 0, 64));
    for (auto it = backgroundCache.begin(); it != backgroundCache.end(); ++it) {
        if (it->path != path || it->fit != fit || it->radius != radius || it->size != m_size)
            continue;
        m_texture = it->texture;
        backgroundCache.splice(backgroundCache.begin(), backgroundCache, it);
        return;
    }
    backgroundCache.push_front({path, fit, radius, m_size, nullptr});
    if (backgroundCache.size() > 4)
        backgroundCache.pop_back();

    try {
        Hyprgraphics::CImage image(path);
        if (!image.success()) {
            Log::logger->log(Log::ERR, Log::logFnName(), "[hyprexpo] Cannot load bg_image '{}': {}", path, image.getError());
            return;
        }
        const auto source = image.cairoSurface();
        if (!source || source->status() != CAIRO_STATUS_SUCCESS)
            return;
        const int width = m_size.x, height = m_size.y;
        using Surface = std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)>;
        using Context = std::unique_ptr<cairo_t, decltype(&cairo_destroy)>;
        Surface surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height), cairo_surface_destroy);
        if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
            return;
        {
            Context cr(cairo_create(surface.get()), cairo_destroy);
            const auto size = source->size();
            const auto box = Hyprexpo::Background::placement(size.x, size.y, width, height, fit);
            if (box.width <= 0 || box.height <= 0)
                return;
            cairo_translate(cr.get(), box.x, box.y);
            cairo_scale(cr.get(), box.width / size.x, box.height / size.y);
            cairo_set_source_surface(cr.get(), source->cairo(), 0, 0);
            cairo_pattern_set_filter(cairo_get_source(cr.get()), CAIRO_FILTER_BILINEAR);
            cairo_rectangle(cr.get(), 0, 0, size.x, size.y);
            cairo_fill(cr.get());
            if (cairo_status(cr.get()) != CAIRO_STATUS_SUCCESS)
                return;
        }
        cairo_surface_flush(surface.get());
        Hyprexpo::Background::blur(cairo_image_surface_get_data(surface.get()), width, height, cairo_image_surface_get_stride(surface.get()), radius);
        cairo_surface_mark_dirty(surface.get());
        m_texture = g_pHyprRenderer->createTexture(surface.get());
        backgroundCache.front().texture = m_texture;
    } catch (const std::exception& error) {
        Log::logger->log(Log::ERR, Log::logFnName(), "[hyprexpo] Cannot prepare bg_image '{}': {}", path, error.what());
    }
}

bool COverviewBackground::render(Render::CRenderContext& ctx, const PHLMONITOR& monitor, float alpha) const {
    if (!monitor || !m_texture)
        return false;
    CRegion damage{0, 0, INT16_MAX, INT16_MAX};
    Render::GL::g_pHyprOpenGL->renderTexture(ctx, m_texture, CBox{{0, 0}, monitor->m_transformedSize}, {.damage = &damage, .a = alpha});
    return true;
}
