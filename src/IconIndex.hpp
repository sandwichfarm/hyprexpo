#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Hyprexpo {

// Where app icons come from, highest priority first.
struct SIconSources {
    std::vector<std::filesystem::path> dataDirs; // <dir>/applications, <dir>/icons/<theme>, <dir>/pixmaps
    std::vector<std::filesystem::path> iconDirs; // extra icon base dirs searched before <dataDir>/icons (e.g. ~/.icons)
    std::vector<std::string>           themes;   // icon themes; hicolor last
};

// XDG data dirs, ~/.icons, and the GTK icon theme followed by hicolor.
SIconSources defaultIconSources();

// Built once from .desktop entries and icon files; lookups afterwards touch no files.
class CIconIndex {
  public:
    explicit CIconIndex(const SIconSources& sources);

    // Icon file for a window class or program name: through the matching .desktop entry (file name,
    // StartupWMClass, or the host of a Chromium --app class found in Exec), else the name itself as an icon.
    std::optional<std::filesystem::path> iconFor(const std::string& name) const;

  private:
    std::optional<std::string>           desktopIconFor(const std::string& name) const;
    std::optional<std::filesystem::path> iconFile(const std::string& iconName) const;

    void                                 indexDesktopEntries(const std::vector<std::filesystem::path>& dataDirs);
    void                                 indexIconFiles(const SIconSources& sources);

    std::unordered_map<std::string, std::string>                        m_iconByName; // lowercased file stem / StartupWMClass
    std::vector<std::pair<std::string, std::string>>                    m_iconByExec; // Exec line, Icon
    std::vector<std::unordered_map<std::string, std::filesystem::path>> m_filesByTier; // per theme, then pixmaps
};

}
