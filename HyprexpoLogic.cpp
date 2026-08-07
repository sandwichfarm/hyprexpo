#include "HyprexpoLogic.hpp"

#include "HyprexpoConfig.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>

namespace Hyprexpo {

std::string trimString(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

std::string lowerString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
    return value;
}

std::vector<std::string> splitCommaList(const std::string& value) {
    std::vector<std::string> entries;
    size_t                   start = 0;

    while (start <= value.size()) {
        size_t comma = value.find(',', start);
        if (comma == std::string::npos)
            comma = value.size();

        entries.push_back(trimString(value.substr(start, comma - start)));

        if (comma == value.size())
            break;
        start = comma + 1;
    }

    return entries;
}

SGridShape computeDynamicGridShape(int visibleCount) {
    if (visibleCount <= 0)
        return {1, 1};

    const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(visibleCount)))));
    int       rows = static_cast<int>(std::ceil(static_cast<double>(visibleCount) / cols));
    if (visibleCount > 1 && rows < 2)
        rows = 2;

    return {cols, rows};
}

std::optional<std::vector<int64_t>> expandDynamicWorkspaceIDs(const std::vector<int64_t>& workspaceIDs, bool fillGaps, std::size_t maxExpandedWorkspaces) {
    std::vector<int64_t> normalized = workspaceIDs;
    std::sort(normalized.begin(), normalized.end());
    normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());

    if (!fillGaps || normalized.empty())
        return normalized;
    if (maxExpandedWorkspaces == 0)
        return std::nullopt;

    const int64_t  minID = normalized.front();
    const int64_t  maxID = normalized.back();
    const __int128 expandedCountWide = static_cast<__int128>(maxID) - static_cast<__int128>(minID) + 1;
    if (expandedCountWide <= 0 || expandedCountWide > static_cast<__int128>(maxExpandedWorkspaces))
        return std::nullopt;

    const std::size_t expandedCount = static_cast<std::size_t>(expandedCountWide);
    std::vector<int64_t> expanded;
    expanded.reserve(expandedCount);
    for (std::size_t offset = 0; offset < expandedCount; ++offset)
        expanded.push_back(minID + static_cast<int64_t>(offset));

    return expanded;
}

SSize aspectCorrectTileSize(double screenW, double screenH, int cols, int rows, double gap) {
    if (screenW <= 0.0 || screenH <= 0.0 || cols <= 0 || rows <= 0)
        return {};

    const double safeGap = std::max(0.0, gap);
    const double monAspect = screenW / screenH;
    const double maxTileW  = std::max(0.0, (screenW - safeGap * (cols - 1)) / cols);
    const double maxTileH  = std::max(0.0, (screenH - safeGap * (rows - 1)) / rows);

    if (maxTileW <= 0.0 || maxTileH <= 0.0 || monAspect <= 0.0)
        return {0.0, 0.0};

    const double cellAspect = maxTileW / maxTileH;
    if (cellAspect > monAspect)
        return {maxTileH * monAspect, maxTileH};

    return {maxTileW, maxTileW / monAspect};
}

STileLayout computeTileLayout(int index, int visibleCount, SGridShape shape, SSize total, double gap, bool centerPartialRows) {
    STileLayout layout;

    if (visibleCount <= 0 || index < 0 || index >= visibleCount)
        return layout;

    shape.cols = std::max(1, shape.cols);
    shape.rows = std::max(1, shape.rows);

    const SSize tileSize = aspectCorrectTileSize(total.w, total.h, shape.cols, shape.rows, gap);
    const int   row      = index / shape.cols;
    const int   col      = index % shape.cols;

    const int    occupiedRows = std::max(1, static_cast<int>(std::ceil(static_cast<double>(visibleCount) / shape.cols)));
    const int    lastRow      = occupiedRows - 1;
    const int    tilesInLastRow = visibleCount % shape.cols == 0 ? shape.cols : visibleCount % shape.cols;
    const double stepX        = tileSize.w + gap;
    const double stepY        = tileSize.h + gap;
    const double gridW        = shape.cols * tileSize.w + (shape.cols - 1) * gap;
    const double gridH        = occupiedRows * tileSize.h + (occupiedRows - 1) * gap;
    const double baseX        = (total.w - gridW) / 2.0;
    const double baseY        = (total.h - gridH) / 2.0;

    double x = baseX + col * stepX;
    const double y = baseY + row * stepY;

    if (centerPartialRows && row == lastRow && tilesInLastRow < shape.cols) {
        const double rowW = tilesInLastRow * tileSize.w + (tilesInLastRow - 1) * gap;
        x                 = (total.w - rowW) / 2.0 + col * stepX;
    }

    layout.box = {x, y, tileSize.w, tileSize.h};
    layout.row = row;
    layout.col = col;
    return layout;
}

int tileIndexAtPoint(double x, double y, int visibleCount, SGridShape shape, SSize total, double gap, bool centerPartialRows) {
    for (int i = 0; i < visibleCount; ++i) {
        const auto layout = computeTileLayout(i, visibleCount, shape, total, gap, centerPartialRows);
        if (x >= layout.box.x && x < layout.box.x + layout.box.w && y >= layout.box.y && y < layout.box.y + layout.box.h)
            return i;
    }

    return -1;
}

int clampGridColumns(int columns) {
    return std::clamp(columns, HyprexpoConfig::COLUMNS_MIN, HyprexpoConfig::COLUMNS_MAX);
}

// For a "first"-anchored square overview grid whose tiles start at
// firstWorkspaceID and count upward, return the grid side length (columns)
// needed so the active workspace is visible. The grid only grows past the
// configured columns; it never shrinks below them and never exceeds
// maxColumns. When even maxColumns cannot reach the active workspace the
// result is clamped to maxColumns (best effort).
int gridColumnsToIncludeWorkspace(int configuredColumns, int firstWorkspaceID, int activeWorkspaceID, int maxColumns) {
    const int cap  = std::max(HyprexpoConfig::COLUMNS_MIN, maxColumns);
    int       cols = std::clamp(configuredColumns, HyprexpoConfig::COLUMNS_MIN, cap);

    // Active workspace is at or before the anchor, so the existing grid already
    // starts on (or after) it; nothing to grow.
    if (activeWorkspaceID <= firstWorkspaceID)
        return cols;

    const long long needed = static_cast<long long>(activeWorkspaceID) - firstWorkspaceID + 1;
    while (static_cast<long long>(cols) * cols < needed && cols < cap)
        ++cols;

    return cols;
}

int tileIndexFromPoint(double x, double y, double width, double height, int sideLength) {
    if (width <= 0 || height <= 0 || sideLength <= 0)
        return -1;

    const int safeSide = clampGridColumns(sideLength);
    const int hx       = std::clamp(static_cast<int>(x / width * safeSide), 0, safeSide - 1);
    const int hy       = std::clamp(static_cast<int>(y / height * safeSide), 0, safeSide - 1);
    return hx + hy * safeSide;
}

int numberKeyToVisibleIndex(int number) {
    if (number == 0)
        return 9;
    if (number < 1 || number > 9)
        return -1;

    return number - 1;
}

ENumberKeyMode numberKeyModeFromString(const std::string& mode) {
    const auto normalized = lowerString(trimString(mode));
    if (normalized == "index")
        return ENumberKeyMode::Index;
    if (normalized == "passthrough")
        return ENumberKeyMode::Passthrough;

    return ENumberKeyMode::Workspace;
}

SDropIntentGeometry computeDropIntentGeometry(const SDropIntentInput& input) {
    SDropIntentGeometry geometry;

    if (!input.targetValid || input.targetTileLocal.w <= 0.0 || input.targetTileLocal.h <= 0.0 || input.workspaceSize.w <= 0.0 || input.workspaceSize.h <= 0.0 || input.windowSize.w <= 0.0 ||
        input.windowSize.h <= 0.0)
        return geometry;

    const double ratioX = std::clamp((input.pointerLocal.x - input.targetTileLocal.x) / input.targetTileLocal.w, 0.0, 1.0);
    const double ratioY = std::clamp((input.pointerLocal.y - input.targetTileLocal.y) / input.targetTileLocal.h, 0.0, 1.0);
    const double scaleX = input.targetTileLocal.w / input.workspaceSize.w;
    const double scaleY = input.targetTileLocal.h / input.workspaceSize.h;

    const double minSize = std::max(0.0, input.minProxySize);
    const double proxyW  = std::clamp(input.windowSize.w * scaleX, std::min(input.targetTileLocal.w, minSize), input.targetTileLocal.w);
    const double proxyH  = std::clamp(input.windowSize.h * scaleY, std::min(input.targetTileLocal.h, minSize), input.targetTileLocal.h);
    const double pointX  = input.targetTileLocal.x + ratioX * input.targetTileLocal.w;
    const double pointY  = input.targetTileLocal.y + ratioY * input.targetTileLocal.h;

    // proxyW/proxyH are clamped to at most the tile size above, so `tile.x + tile.w - proxyW` is
    // algebraically >= tile.x. In floating point it is not: when the dragged window is at least as
    // large as the tile the proxy clamps to exactly tile.w/tile.h, and the subtraction can land an
    // ULP below tile.x. Passing that as clamp()'s upper bound is UB, and libstdc++ builds with
    // assertions enabled turn it into an abort() that takes the whole compositor down mid-render.
    const double maxProxyX = std::max(input.targetTileLocal.x, input.targetTileLocal.x + input.targetTileLocal.w - proxyW);
    const double maxProxyY = std::max(input.targetTileLocal.y, input.targetTileLocal.y + input.targetTileLocal.h - proxyH);

    geometry.valid                = true;
    geometry.targetWorkspacePoint = {ratioX * input.workspaceSize.w, ratioY * input.workspaceSize.h};
    geometry.targetProxyLocal     = {
        std::clamp(pointX - input.grabOffset.x * scaleX, input.targetTileLocal.x, maxProxyX),
        std::clamp(pointY - input.grabOffset.y * scaleY, input.targetTileLocal.y, maxProxyY),
        proxyW,
        proxyH,
    };

    return geometry;
}

SGestureSyncDecision evaluateGestureSync(const SGestureConfig& config) {
    if (config.fingers == 0)
        return {};

    if (config.fingers < 2 || config.fingers > 9)
        return {.error = "gesture_fingers must be 0 (disabled) or 2-9, got " + std::to_string(config.fingers)};

    if (!config.directionValid)
        return {.error = "gesture_direction '" + config.direction + "' is not a valid trackpad direction"};

    return {.registerGesture = true, .error = ""};
}

// Hyprland 0.56 exposes plugin strings as const char*.
std::string decodeConfigString(const void* dataptr, bool underlyingIsStdString, const std::string& fallback) {
    if (!dataptr)
        return fallback;

    if (underlyingIsStdString) {
        auto* const* ptr = reinterpret_cast<std::string* const*>(dataptr);
        return ptr && *ptr ? **ptr : fallback;
    }

    auto* const* ptr = reinterpret_cast<const char* const*>(dataptr);
    return ptr && *ptr ? std::string{*ptr} : fallback;
}

std::string fallbackTokenForVisibleIndex(int visibleIndex) {
    if (visibleIndex < 0)
        return "";
    if (visibleIndex < 9)
        return std::to_string(visibleIndex + 1);
    if (visibleIndex == 9)
        return "0";
    if (visibleIndex < 36)
        return std::string(1, static_cast<char>('a' + visibleIndex - 10));

    return "";
}

int fallbackTokenToVisibleIndex(const std::string& token) {
    const auto normalized = lowerString(trimString(token));
    if (normalized.size() != 1)
        return -1;

    const char c = normalized[0];
    if (c >= '1' && c <= '9')
        return c - '1';
    if (c == '0')
        return 9;
    if (c >= 'a' && c <= 'z')
        return 10 + c - 'a';

    return -1;
}

static int hexTo(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    return -1;
}

static bool parseHexByte(const std::string& value, size_t index, int& out) {
    if (index + 1 >= value.size())
        return false;

    const int hi = hexTo(value[index]);
    const int lo = hexTo(value[index + 1]);
    if (hi < 0 || lo < 0)
        return false;

    out = (hi << 4) | lo;
    return true;
}

bool parseHexRGBA8(const std::string& value, SColorRGBA& out) {
    const std::string hex = trimString(value);
    if (hex.size() != 8)
        return false;

    int r = 0, g = 0, b = 0, a = 0;
    if (!parseHexByte(hex, 0, r) || !parseHexByte(hex, 2, g) || !parseHexByte(hex, 4, b) || !parseHexByte(hex, 6, a))
        return false;

    out = SColorRGBA{r / 255.F, g / 255.F, b / 255.F, a / 255.F};
    return true;
}

static bool parseHexARGB8(const std::string& value, SColorRGBA& out) {
    const std::string hex = trimString(value);
    if (hex.size() != 8)
        return false;

    int a = 0, r = 0, g = 0, b = 0;
    if (!parseHexByte(hex, 0, a) || !parseHexByte(hex, 2, r) || !parseHexByte(hex, 4, g) || !parseHexByte(hex, 6, b))
        return false;

    out = SColorRGBA{r / 255.F, g / 255.F, b / 255.F, a / 255.F};
    return true;
}

bool parseSolidColorSpec(const std::string& value, SColorRGBA& out) {
    std::string spec = trimString(value);
    if (spec.empty())
        return false;

    const std::string lowered = lowerString(spec);
    if (lowered.starts_with("rgb(") && spec.ends_with(")")) {
        const auto hex = spec.substr(4, spec.size() - 5);
        return parseHexRGBA8(hex + "ff", out);
    }

    if (lowered.starts_with("rgba(") && spec.ends_with(")")) {
        const auto hex = spec.substr(5, spec.size() - 6);
        return parseHexRGBA8(hex, out);
    }

    if (lowered.starts_with("0x"))
        spec = spec.substr(2);

    return parseHexARGB8(spec, out);
}

SGradientSpec parseGradientSpec(const std::string& value) {
    SGradientSpec spec;
    std::string   normalized = value;
    normalized.erase(std::remove(normalized.begin(), normalized.end(), ','), normalized.end());

    const auto p1 = normalized.find("rgba(");
    const auto p2 = normalized.find("rgba(", p1 == std::string::npos ? 0 : p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos)
        return spec;

    const auto e1 = normalized.find(')', p1);
    const auto e2 = normalized.find(')', p2);
    if (e1 == std::string::npos || e2 == std::string::npos)
        return spec;

    if (!parseHexRGBA8(normalized.substr(p1 + 5, e1 - (p1 + 5)), spec.c1) || !parseHexRGBA8(normalized.substr(p2 + 5, e2 - (p2 + 5)), spec.c2))
        return spec;

    const auto deg = normalized.find("deg", e2);
    if (deg != std::string::npos) {
        size_t begin = normalized.rfind(' ', deg);
        if (begin == std::string::npos)
            begin = e2 + 1;
        else
            begin += 1;

        float parsed = 0.F;
        const auto angle = trimString(normalized.substr(begin, deg - begin));
        const auto res   = std::from_chars(angle.data(), angle.data() + angle.size(), parsed);
        if (res.ec == std::errc{} && res.ptr == angle.data() + angle.size())
            spec.angleDeg = parsed;
    }

    spec.valid = true;
    return spec;
}

bool isGradientBorderSpec(const std::string& value) {
    const auto first = value.find("rgba(");
    return first != std::string::npos && value.find("rgba(", first + 1) != std::string::npos;
}

bool shouldShowWorkspaceLabel(bool labelEnabled, const std::string& labelShow, bool isHovered, bool isFocused, bool isCurrent) {
    if (!labelEnabled)
        return false;

    const auto mode = lowerString(trimString(labelShow));
    if (mode == "never")
        return false;
    if (mode == "hover")
        return isHovered;
    if (mode == "focus")
        return isFocused;
    if (mode == "hover+focus")
        return isHovered || isFocused;
    if (mode == "current+focus")
        return isCurrent || isFocused;

    return true;
}

std::string resolveBorderSpec(const std::string& modernSpec, const std::string& legacySpec) {
    const auto modern = trimString(modernSpec);
    return modern.empty() ? trimString(legacySpec) : modern;
}

static std::vector<std::string> splitWhitespace(const std::string& value) {
    std::vector<std::string> tokens;
    size_t                   cursor = 0;

    while (cursor < value.size()) {
        while (cursor < value.size() && std::isspace(static_cast<unsigned char>(value[cursor])))
            ++cursor;
        const size_t begin = cursor;
        while (cursor < value.size() && !std::isspace(static_cast<unsigned char>(value[cursor])))
            ++cursor;
        if (begin < cursor)
            tokens.push_back(value.substr(begin, cursor - begin));
    }

    return tokens;
}

SWorkspaceMethodSpec parseWorkspaceMethodSpec(const std::string& method) {
    SWorkspaceMethodSpec spec;
    const auto           tokens = splitWhitespace(method);

    if (tokens.size() != 2) {
        spec.error = "expected '<center|first> <workspace>'";
        return spec;
    }

    const auto mode = lowerString(tokens[0]);
    if (mode == "center")
        spec.mode = EWorkspaceMethodMode::Center;
    else if (mode == "first")
        spec.mode = EWorkspaceMethodMode::First;
    else {
        spec.error = "expected workspace method 'center' or 'first'";
        return spec;
    }

    if (tokens[1].empty()) {
        spec.error = "workspace token cannot be empty";
        return spec;
    }

    spec.workspace = tokens[1];
    spec.valid     = true;
    return spec;
}

SWorkspaceMethodSpec resolveWorkspaceMethodForMonitor(const std::string& config, const std::string& monitorName) {
    const std::string trimmed = trimString(config);
    if (trimmed.empty())
        return parseWorkspaceMethodSpec(HyprexpoConfig::WORKSPACE_METHOD_DEFAULT);

    std::string globalFallback;
    for (const auto& entry : splitCommaList(trimmed)) {
        if (entry.empty())
            continue;

        const auto tokens = splitWhitespace(entry);
        if (tokens.size() == 3) {
            if (tokens[0] == monitorName)
                return parseWorkspaceMethodSpec(tokens[1] + " " + tokens[2]);
            continue;
        }

        if (tokens.size() == 2 && globalFallback.empty())
            globalFallback = entry;
    }

    if (!globalFallback.empty())
        return parseWorkspaceMethodSpec(globalFallback);

    auto invalid = parseWorkspaceMethodSpec(trimmed);
    if (!invalid.valid && invalid.error.empty())
        invalid.error = "invalid workspace method config";
    return invalid;
}

}
