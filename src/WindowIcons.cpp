#include "WindowIcons.hpp"
#include "HyprexpoLogic.hpp"

#include <hyprgraphics/image/Image.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::unordered_map<std::string, SP<Render::ITexture>> g_iconCache;

// ---- XDG locations ------------------------------------------------------------------------------

std::vector<fs::path> dataDirs() {
    std::vector<fs::path> dirs;
    if (const char* dataHome = std::getenv("XDG_DATA_HOME"); dataHome && *dataHome)
        dirs.emplace_back(dataHome);
    else if (const char* home = std::getenv("HOME"))
        dirs.emplace_back(fs::path(home) / ".local/share");

    const char*       system = std::getenv("XDG_DATA_DIRS");
    const std::string list   = system && *system ? system : "/usr/local/share:/usr/share";
    for (size_t start = 0; start <= list.size();) {
        const size_t end = std::min(list.find(':', start), list.size());
        if (end > start)
            dirs.emplace_back(list.substr(start, end - start));
        start = end + 1;
    }
    return dirs;
}

// Key=value pairs of one group in an ini-style file (.desktop, index.theme, gtk settings.ini).
std::unordered_map<std::string, std::string> iniGroup(const fs::path& path, const std::string& group) {
    std::unordered_map<std::string, std::string> values;
    std::ifstream                                file(path);
    bool                                         inGroup = false;
    for (std::string line; std::getline(file, line);) {
        line = Hyprexpo::trimString(line);
        if (line.starts_with('[')) {
            inGroup = line == "[" + group + "]";
            continue;
        }
        const auto EQ = line.find('=');
        if (inGroup && EQ != std::string::npos)
            values.try_emplace(Hyprexpo::trimString(line.substr(0, EQ)), Hyprexpo::trimString(line.substr(EQ + 1)));
    }
    return values;
}

// ---- class -> icon name -------------------------------------------------------------------------

std::optional<std::string> desktopIconFor(const std::string& windowClass) {
    const auto WANT = Hyprexpo::lowerString(windowClass);
    const auto HOST = Hyprexpo::chromiumAppHost(windowClass);
    if (WANT.empty())
        return std::nullopt;

    for (const auto& dir : dataDirs()) {
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator(dir / "applications", fs::directory_options::skip_permission_denied, ec)) {
            if (entry.path().extension() != ".desktop")
                continue;

            auto       desktop = iniGroup(entry.path(), "Desktop Entry");
            const bool MATCH   = Hyprexpo::lowerString(entry.path().stem().string()) == WANT || Hyprexpo::lowerString(desktop["StartupWMClass"]) == WANT ||
                (!HOST.empty() && desktop["Exec"].find(HOST) != std::string::npos);
            if (MATCH && !desktop["Icon"].empty())
                return desktop["Icon"];
        }
    }
    return std::nullopt;
}

// Windows with a custom class and no .desktop entry (e.g. `foot --app-id=...`) fall back to their program.
std::string processName(pid_t pid) {
    std::ifstream file("/proc/" + std::to_string(pid) + "/comm");
    std::string   name;
    std::getline(file, name);
    return name;
}

std::string iconNameFor(const PHLWINDOW& window) {
    if (const auto ICON = desktopIconFor(window->m_class))
        return *ICON;
    if (const auto ICON = desktopIconFor(processName(window->getPID())))
        return *ICON;
    return window->m_class;
}

// ---- icon name -> file --------------------------------------------------------------------------

std::string gtkIconTheme() {
    const char* home = std::getenv("HOME");
    if (!home)
        return "";
    for (const auto* file : {".config/gtk-3.0/settings.ini", ".config/gtk-4.0/settings.ini"}) {
        if (auto theme = iniGroup(fs::path(home) / file, "Settings")["gtk-icon-theme-name"]; !theme.empty())
            return theme;
    }
    return "";
}

// shortcut: themes are searched without following their Inherits chain; hicolor and pixmaps cover most apps.
std::optional<fs::path> iconFile(const std::string& iconName) {
    if (fs::path direct{iconName}; direct.is_absolute())
        return fs::exists(direct) ? std::optional{direct} : std::nullopt;

    std::vector<fs::path> iconDirs;
    if (const char* home = std::getenv("HOME"))
        iconDirs.push_back(fs::path(home) / ".icons");
    for (const auto& dir : dataDirs())
        iconDirs.push_back(dir / "icons");

    for (const auto& theme : {gtkIconTheme(), std::string{"hicolor"}}) {
        if (theme.empty())
            continue;

        // One index.theme (usually the system one) lists the subdirectories for every base dir of the theme.
        std::vector<std::string> subdirs;
        for (const auto& base : iconDirs) {
            if (const auto INDEX = base / theme / "index.theme"; fs::exists(INDEX)) {
                subdirs = Hyprexpo::splitCommaList(iniGroup(INDEX, "Icon Theme")["Directories"]);
                break;
            }
        }

        // Prefer scalable art, else the largest bitmap.
        std::optional<fs::path> best;
        int                     bestScore = -1;
        for (const auto& base : iconDirs) {
            const auto THEMEDIR = base / theme;
            for (const auto& sub : subdirs) {
                for (const auto* ext : {".svg", ".png"}) {
                    const auto CANDIDATE = THEMEDIR / sub / (iconName + ext);
                    if (!fs::exists(CANDIDATE))
                        continue;
                    const int SCORE = std::string_view{ext} == ".svg" ? 1 << 20 : std::atoi(sub.c_str());
                    if (SCORE > bestScore) {
                        best      = CANDIDATE;
                        bestScore = SCORE;
                    }
                }
            }
        }
        if (best)
            return best;
    }

    for (const auto& dir : dataDirs()) {
        for (const auto* ext : {".svg", ".png"}) {
            if (const auto CANDIDATE = dir / "pixmaps" / (iconName + ext); fs::exists(CANDIDATE))
                return CANDIDATE;
        }
    }
    return std::nullopt;
}

SP<Render::ITexture> loadTexture(const fs::path& path, int sizePx) {
    Hyprgraphics::CImage image(path.string(), Vector2D{(double)sizePx, (double)sizePx});
    if (!image.success() || !image.cairoSurface() || !image.cairoSurface()->cairo())
        return nullptr;
    return g_pHyprRenderer->createTexture(image.cairoSurface()->cairo());
}
}

SP<Render::ITexture> windowIconTexture(const PHLWINDOW& window, int sizePx) {
    const auto KEY = window->m_class + "\n" + std::to_string(sizePx);
    if (const auto IT = g_iconCache.find(KEY); IT != g_iconCache.end())
        return IT->second;

    const auto PATH = iconFile(iconNameFor(window));
    return g_iconCache[KEY] = PATH ? loadTexture(*PATH, sizePx) : nullptr;
}

void clearWindowIconCache() {
    g_iconCache.clear();
}
