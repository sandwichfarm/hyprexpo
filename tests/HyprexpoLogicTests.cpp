#include "../HyprexpoLogic.hpp"
#include "../HyprexpoConfig.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& label) {
    if (condition)
        return;

    ++failures;
    std::cerr << "FAIL: " << label << '\n';
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.001F;
}

bool near(double a, double b) {
    return std::abs(a - b) < 0.001;
}

}

int main() {
    using namespace Hyprexpo;

    expect(trimString("  DP-1 first 1 \t") == "DP-1 first 1", "trimString removes surrounding whitespace");
    expect(splitCommaList("a, b,,c").size() == 4, "splitCommaList preserves empty entries");

    expect(clampGridColumns(-1) == 1, "columns clamp lower bound");
    expect(clampGridColumns(3) == 3, "columns keep valid value");
    expect(clampGridColumns(99) == 7, "columns clamp upper bound");
    expect(HyprexpoConfig::SHOW_PINNED_WINDOWS_DEFAULT == 0, "pinned windows are hidden from previews by default");
    expect(HyprexpoConfig::DRAG_DROP_ENABLE_DEFAULT == 1, "drag and drop is enabled by default");

    expect(tileIndexFromPoint(0, 0, 300, 300, 3) == 0, "tile index top-left");
    expect(tileIndexFromPoint(299, 299, 300, 300, 3) == 8, "tile index bottom-right inside");
    expect(tileIndexFromPoint(300, 300, 300, 300, 3) == 8, "tile index clamps monitor edge");
    expect(tileIndexFromPoint(10, 10, 0, 300, 3) == -1, "tile index rejects invalid width");

    SDropIntentInput dropInput{
        .targetValid     = true,
        .pointerLocal    = {150, 150},
        .targetTileLocal = {0, 0, 300, 300},
        .workspaceSize   = {1200, 900},
        .windowSize      = {300, 180},
        .grabOffset      = {150, 90},
    };
    auto drop = computeDropIntentGeometry(dropInput);
    expect(drop.valid, "drop intent center is valid");
    expect(near(drop.targetWorkspacePoint.x, 600) && near(drop.targetWorkspacePoint.y, 450), "drop intent maps pointer to workspace point");
    expect(near(drop.targetProxyLocal.x, 112.5) && near(drop.targetProxyLocal.y, 120), "drop intent preserves grab offset");
    expect(near(drop.targetProxyLocal.w, 75) && near(drop.targetProxyLocal.h, 60), "drop intent scales window into target preview");

    dropInput.pointerLocal = {300, 300};
    drop                   = computeDropIntentGeometry(dropInput);
    expect(near(drop.targetWorkspacePoint.x, 1200) && near(drop.targetWorkspacePoint.y, 900), "drop intent maps bottom-right edge");
    expect(near(drop.targetProxyLocal.x, 225) && near(drop.targetProxyLocal.y, 240), "drop intent clamps bottom-right proxy");

    dropInput.pointerLocal = {-20, -20};
    drop                   = computeDropIntentGeometry(dropInput);
    expect(near(drop.targetWorkspacePoint.x, 0) && near(drop.targetWorkspacePoint.y, 0), "drop intent clamps outside pointer to workspace edge");
    expect(near(drop.targetProxyLocal.x, 0) && near(drop.targetProxyLocal.y, 0), "drop intent clamps outside pointer proxy to tile edge");

    dropInput.targetValid = false;
    expect(!computeDropIntentGeometry(dropInput).valid, "drop intent rejects invalid target");
    dropInput.targetValid  = true;
    dropInput.windowSize.w = 0;
    expect(!computeDropIntentGeometry(dropInput).valid, "drop intent rejects invalid window size");

    expect(fallbackTokenForVisibleIndex(0) == "1", "fallback token first workspace");
    expect(fallbackTokenForVisibleIndex(9) == "0", "fallback token tenth workspace");
    expect(fallbackTokenForVisibleIndex(10) == "a", "fallback token alpha start");
    expect(fallbackTokenToVisibleIndex("A") == 10, "fallback token accepts uppercase alpha");
    expect(fallbackTokenToVisibleIndex("zz") == -1, "fallback token rejects multi-character fallback");

    SColorRGBA color;
    expect(parseHexRGBA8("33ccffee", color), "parse rgba hex");
    expect(near(color.r, 0x33 / 255.F) && near(color.a, 0xee / 255.F), "rgba channels are in rrggbbaa order");
    expect(parseSolidColorSpec("rgb(66ccff)", color), "parse rgb solid color");
    expect(near(color.r, 0x66 / 255.F) && near(color.a, 1.F), "rgb solid color is opaque");
    expect(parseSolidColorSpec("0x8066ccff", color), "parse argb solid color");
    expect(near(color.a, 0x80 / 255.F) && near(color.r, 0x66 / 255.F), "argb solid channel order");
    expect(!parseSolidColorSpec("rgb(nothex)", color), "invalid rgb solid rejected");

    const auto gradient = parseGradientSpec("rgba(33ccffee) rgba(00ff99ee) 45deg");
    expect(gradient.valid, "gradient parses two rgba colors");
    expect(near(gradient.angleDeg, 45.F), "gradient angle parses");
    expect(!parseGradientSpec("rgba(33ccffee) nope 45deg").valid, "invalid gradient rejected");
    expect(isGradientBorderSpec("rgba(33ccffee) rgba(00ff99ee) 45deg"), "gradient border detected");

    auto method = parseWorkspaceMethodSpec("center current");
    expect(method.valid && method.mode == EWorkspaceMethodMode::Center && method.workspace == "current", "global center method parses");
    method = parseWorkspaceMethodSpec("first 9");
    expect(method.valid && method.mode == EWorkspaceMethodMode::First && method.workspace == "9", "first method parses");
    expect(!parseWorkspaceMethodSpec("middle 9").valid, "invalid workspace method rejected");
    method = resolveWorkspaceMethodForMonitor("DP-1 first 1, HDMI-A-1 center 9, center current", "HDMI-A-1");
    expect(method.valid && method.mode == EWorkspaceMethodMode::Center && method.workspace == "9", "per-monitor method wins");
    method = resolveWorkspaceMethodForMonitor("DP-1 first 1, center current", "eDP-1");
    expect(method.valid && method.mode == EWorkspaceMethodMode::Center && method.workspace == "current", "global fallback method applies");

    if (failures != 0)
        return 1;

    std::cout << "HyprexpoLogicTests passed\n";
    return 0;
}
