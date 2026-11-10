#include "IconIndex.hpp"

#include "HyprexpoLogic.hpp"

#include <cstdlib>
#include <fstream>

namespace fs = std::filesystem;

namespace Hyprexpo {

namespace {

// Key=value pairs of one group in an ini-style file (.desktop, gtk settings.ini).
std::unordered_map<std::string, std::string> iniGroup(const fs::path& path, const std::string& group) {
    std::unordered_map<std::string, std::string> values;
    std::ifstream                                file(path);
    bool                                         inGroup = false;
    for (std::string line; std::getline(file, line);) {
        line = trimString(line);
        if (line.starts_with('[')) {
            inGroup = line == "[" + group + "]";
            continue;
        }
        const auto EQ = line.find('=');
        if (inGroup && EQ != std::string::npos)
            values.try_emplace(trimString(line.substr(0, EQ)), trimString(line.substr(EQ + 1)));
    }
    return values;
}

std::vector<fs::path> splitPathList(const char* value, const char* fallback) {
    std::vector<fs::path> dirs;
    const std::string     list = value && *value ? value : fallback;
    for (size_t start = 0; start <= list.size();) {
        const size_t end = std::min(list.find(':', start), list.size());
        if (end > start)
            dirs.emplace_back(list.substr(start, end - start));
        start = end + 1;
    }
    return dirs;
}

bool isIconFile(const fs::path& path) {
    return path.extension() == ".svg" || path.extension() == ".png";
}

// Scalable art first, then the largest bitmap (size taken from an "NxN" directory in the path).
int iconScore(const fs::path& path) {
    if (path.extension() == ".svg")
        return 1 << 20;
    int size = 0;
    for (const auto& part : path)
        size = std::max(size, std::atoi(part.c_str()));
    return size;
}

void keepBest(std::unordered_map<std::string, fs::path>& files, const fs::path& candidate) {
    auto [it, inserted] = files.try_emplace(candidate.stem().string(), candidate);
    if (!inserted && iconScore(candidate) > iconScore(it->second))
        it->second = candidate;
}

}

SIconSources defaultIconSources() {
    SIconSources sources;
    const char*  home = std::getenv("HOME");

    if (const char* dataHome = std::getenv("XDG_DATA_HOME"); dataHome && *dataHome)
        sources.dataDirs.emplace_back(dataHome);
    else if (home)
        sources.dataDirs.push_back(fs::path(home) / ".local/share");
    for (auto& dir : splitPathList(std::getenv("XDG_DATA_DIRS"), "/usr/local/share:/usr/share"))
        sources.dataDirs.push_back(std::move(dir));

    if (home) {
        sources.iconDirs.push_back(fs::path(home) / ".icons");
        for (const auto* file : {".config/gtk-3.0/settings.ini", ".config/gtk-4.0/settings.ini"}) {
            if (auto theme = iniGroup(fs::path(home) / file, "Settings")["gtk-icon-theme-name"]; !theme.empty() && theme != "hicolor") {
                sources.themes.push_back(theme);
                break;
            }
        }
    }
    sources.themes.emplace_back("hicolor");
    return sources;
}

CIconIndex::CIconIndex(const SIconSources& sources) {
    indexDesktopEntries(sources.dataDirs);
    indexIconFiles(sources);
}

std::optional<fs::path> CIconIndex::iconFor(const std::string& name) const {
    if (name.empty())
        return std::nullopt;
    if (const auto ICON = desktopIconFor(name))
        return iconFile(*ICON);
    // App IDs are client-supplied names, never paths to open on the host.
    if (fs::path{name}.has_parent_path())
        return std::nullopt;
    return iconFile(name);
}

std::optional<std::string> CIconIndex::desktopIconFor(const std::string& name) const {
    if (const auto IT = m_iconByName.find(lowerString(name)); IT != m_iconByName.end())
        return IT->second;

    if (const auto HOST = chromiumAppHost(name); !HOST.empty()) {
        for (const auto& [exec, icon] : m_iconByExec) {
            if (exec.find(HOST) != std::string::npos)
                return icon;
        }
    }
    return std::nullopt;
}

std::optional<fs::path> CIconIndex::iconFile(const std::string& iconName) const {
    if (fs::path direct{iconName}; direct.is_absolute())
        return direct; // regular-file status checked when the desktop entry was indexed

    for (const auto& files : m_filesByTier) {
        if (const auto IT = files.find(iconName); IT != files.end())
            return IT->second;
    }
    return std::nullopt;
}

void CIconIndex::indexDesktopEntries(const std::vector<fs::path>& dataDirs) {
    // Earlier data dirs win, as in XDG lookup.
    for (const auto& dir : dataDirs) {
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(dir / "applications", fs::directory_options::skip_permission_denied, ec);
             !ec && it != fs::recursive_directory_iterator{}; it.increment(ec)) {
            const auto&     entry = *it;
            std::error_code entryError;
            if (entry.path().extension() != ".desktop" || !entry.is_regular_file(entryError))
                continue;

            auto        desktop = iniGroup(entry.path(), "Desktop Entry");
            const auto& icon    = desktop["Icon"];
            if (icon.empty() || (fs::path{icon}.is_absolute() && !fs::is_regular_file(icon, entryError)))
                continue;

            m_iconByName.try_emplace(lowerString(entry.path().stem().string()), icon);
            if (const auto& wmClass = desktop["StartupWMClass"]; !wmClass.empty())
                m_iconByName.try_emplace(lowerString(wmClass), icon);
            if (const auto& exec = desktop["Exec"]; !exec.empty())
                m_iconByExec.emplace_back(exec, icon);
        }
    }
}

void CIconIndex::indexIconFiles(const SIconSources& sources) {
    std::vector<fs::path> bases = sources.iconDirs;
    for (const auto& dir : sources.dataDirs)
        bases.push_back(dir / "icons");

    // shortcut: a theme's Inherits chain isn't followed; the GTK theme, hicolor and pixmaps cover most apps.
    for (const auto& theme : sources.themes) {
        auto& files = m_filesByTier.emplace_back();
        for (const auto& base : bases) {
            std::error_code ec;
            for (auto it = fs::recursive_directory_iterator(base / theme, fs::directory_options::skip_permission_denied, ec);
                 !ec && it != fs::recursive_directory_iterator{}; it.increment(ec)) {
                const auto&     entry = *it;
                std::error_code entryError;
                if (isIconFile(entry.path()) && entry.is_regular_file(entryError))
                    keepBest(files, entry.path());
            }
        }
    }

    auto& pixmaps = m_filesByTier.emplace_back();
    for (const auto& dir : sources.dataDirs) {
        std::error_code ec;
        for (auto it = fs::directory_iterator(dir / "pixmaps", fs::directory_options::skip_permission_denied, ec);
             !ec && it != fs::directory_iterator{}; it.increment(ec)) {
            const auto&     entry = *it;
            std::error_code entryError;
            if (isIconFile(entry.path()) && entry.is_regular_file(entryError))
                keepBest(pixmaps, entry.path());
        }
    }
}

}
