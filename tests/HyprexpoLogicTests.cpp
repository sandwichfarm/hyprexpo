#include "../HyprexpoLogic.hpp"
#include "../HyprexpoConfig.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& label) {
    if (condition)
        return;

    ++failures;
    std::cerr << "FAIL: " << label << '\n';
}

bool near(double a, double b, double eps = 0.001) {
    return std::abs(a - b) < eps;
}

Hyprexpo::SSize makeSize(double w, double h) {
    return {w, h};
}

std::vector<Hyprexpo::STileLayout> layoutsFor(int count, const Hyprexpo::SGridShape& shape, const Hyprexpo::SSize& total, double gap, bool centerPartialRows) {
    std::vector<Hyprexpo::STileLayout> layouts;
    for (int i = 0; i < count; ++i)
        layouts.push_back(Hyprexpo::computeTileLayout(i, count, shape, total, gap, centerPartialRows));
    return layouts;
}

void checkGeometryForMonitor(const Hyprexpo::SSize& total) {
    constexpr double gap = 24.0;

    for (int count = 1; count <= 10; ++count) {
        const auto shape = Hyprexpo::computeDynamicGridShape(count);
        const int  expectedCols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
        int        expectedRows = static_cast<int>(std::ceil(static_cast<double>(count) / expectedCols));
        if (count > 1 && expectedRows < 2)
            expectedRows = 2;

        expect(shape.cols == expectedCols, "dynamic cols for count " + std::to_string(count));
        expect(shape.rows == expectedRows, "dynamic rows for count " + std::to_string(count));

        const auto tile = Hyprexpo::aspectCorrectTileSize(total.w, total.h, shape.cols, shape.rows, gap);
        const double monitorAspect = total.w / total.h;
        expect(near(tile.w / tile.h, monitorAspect), "tile aspect matches monitor aspect for count " + std::to_string(count));

        const double maxTileW = std::max(0.0, (total.w - gap * (shape.cols - 1)) / shape.cols);
        const double maxTileH = std::max(0.0, (total.h - gap * (shape.rows - 1)) / shape.rows);
        expect(tile.w <= maxTileW + 0.001 && tile.h <= maxTileH + 0.001, "tile fits within cell for count " + std::to_string(count));

        const auto layouts = layoutsFor(count, shape, total, gap, true);
        const int occupiedRows = std::max(1, static_cast<int>(std::ceil(static_cast<double>(count) / shape.cols)));
        const double gridW = shape.cols * tile.w + (shape.cols - 1) * gap;
        const double gridH = occupiedRows * tile.h + (occupiedRows - 1) * gap;
        const double baseX = (total.w - gridW) / 2.0;
        const double baseY = (total.h - gridH) / 2.0;

        double minX = layouts.front().box.x;
        double minY = layouts.front().box.y;
        double maxX = layouts.front().box.x + layouts.front().box.w;
        double maxY = layouts.front().box.y + layouts.front().box.h;
        for (const auto& layout : layouts) {
            minX = std::min(minX, layout.box.x);
            minY = std::min(minY, layout.box.y);
            maxX = std::max(maxX, layout.box.x + layout.box.w);
            maxY = std::max(maxY, layout.box.y + layout.box.h);
        }

        expect(near(minX, baseX), "grid centered horizontally for count " + std::to_string(count));
        expect(near(minY, baseY), "grid centered vertically for count " + std::to_string(count));
        expect(near(maxX - minX, gridW), "grid width consistent for count " + std::to_string(count));
        expect(near(maxY - minY, gridH), "grid height consistent for count " + std::to_string(count));

        for (int row = 0; row < occupiedRows; ++row) {
            std::vector<const Hyprexpo::STileLayout*> rowTiles;
            for (const auto& layout : layouts) {
                if (layout.row == row)
                    rowTiles.push_back(&layout);
            }
            std::sort(rowTiles.begin(), rowTiles.end(), [](const auto* a, const auto* b) { return a->col < b->col; });

            for (size_t i = 1; i < rowTiles.size(); ++i) {
                const auto& left = *rowTiles[i - 1];
                const auto& right = *rowTiles[i];
                expect(near(right.box.x - (left.box.x + left.box.w), gap), "uniform horizontal gap for count " + std::to_string(count));
            }
        }

        for (int row = 1; row < occupiedRows; ++row) {
            for (int col = 0; col < shape.cols; ++col) {
                const int upperIndex = row * shape.cols + col;
                const int lowerIndex = (row - 1) * shape.cols + col;
                if (upperIndex >= count || lowerIndex >= count)
                    continue;

                const auto& upper = layouts[lowerIndex];
                const auto& lower = layouts[upperIndex];
                expect(near(lower.box.y - (upper.box.y + upper.box.h), gap), "uniform vertical gap for count " + std::to_string(count));
            }
        }

        for (int i = 0; i < count; ++i) {
            const auto& box = layouts[i].box;
            const double centerX = box.x + box.w / 2.0;
            const double centerY = box.y + box.h / 2.0;
            expect(Hyprexpo::tileIndexAtPoint(centerX, centerY, count, shape, total, gap, true) == i, "hit test returns tile center for count " + std::to_string(count));
        }

        if (shape.cols > 1) {
            const auto& left = layouts[0].box;
            const double gapX = left.x + left.w + gap / 2.0;
            const double gapY = left.y + left.h / 2.0;
            expect(Hyprexpo::tileIndexAtPoint(gapX, gapY, count, shape, total, gap, true) == -1, "hit test rejects tile gap for count " + std::to_string(count));
        }

        const int lastRowCount = count % shape.cols == 0 ? shape.cols : count % shape.cols;
        if (lastRowCount < shape.cols && count > shape.cols) {
            const int lastRow = occupiedRows - 1;
            std::vector<const Hyprexpo::STileLayout*> rowTiles;
            for (const auto& layout : layouts) {
                if (layout.row == lastRow)
                    rowTiles.push_back(&layout);
            }
            std::sort(rowTiles.begin(), rowTiles.end(), [](const auto* a, const auto* b) { return a->col < b->col; });

            const double rowMinX = rowTiles.front()->box.x;
            const double rowMaxX = rowTiles.back()->box.x + rowTiles.back()->box.w;
            expect(near((rowMinX - minX), (maxX - rowMaxX)), "partial row is centered for count " + std::to_string(count));

            const double emptyX = rowMinX - gap / 2.0;
            const double emptyY = rowTiles.front()->box.y + rowTiles.front()->box.h / 2.0;
            expect(Hyprexpo::tileIndexAtPoint(emptyX, emptyY, count, shape, total, gap, true) == -1, "hit test rejects centered empty region for count " + std::to_string(count));
        }

        if (count == 2) {
            expect(shape.rows == 2, "count 2 keeps two-row grid shape");
            expect(near(layouts[0].box.y, layouts[1].box.y), "count 2 stays on one occupied row");
            expect(near(layouts[0].box.y, (total.h - tile.h) / 2.0), "count 2 row is vertically centered");
        }
    }
}

}

int main() {
    using namespace Hyprexpo;

    expect(trimString("  DP-1 first 1 \t") == "DP-1 first 1", "trimString removes surrounding whitespace");
    expect(splitCommaList("a, b,,c").size() == 4, "splitCommaList preserves empty entries");

    expect(clampGridColumns(-1) == 1, "columns clamp lower bound");
    expect(clampGridColumns(3) == 3, "columns keep valid value");
    expect(clampGridColumns(99) == 7, "columns clamp upper bound");

    expect(gridColumnsToIncludeWorkspace(3, 1, 9, 7) == 3, "first-anchor grid keeps columns when active workspace fits");
    expect(gridColumnsToIncludeWorkspace(3, 1, 10, 7) == 4, "first-anchor grid grows to include an active workspace just past the grid");
    expect(gridColumnsToIncludeWorkspace(3, 1, 16, 7) == 4, "first-anchor grid grows to exactly fill the last tile");
    expect(gridColumnsToIncludeWorkspace(3, 1, 17, 7) == 5, "first-anchor grid grows another column past a full grid");
    expect(gridColumnsToIncludeWorkspace(3, 5, 12, 7) == 3, "first-anchor grid accounts for a non-1 anchor when sizing");
    expect(gridColumnsToIncludeWorkspace(3, 5, 4, 7) == 3, "first-anchor grid never shrinks for an active workspace before the anchor");
    expect(gridColumnsToIncludeWorkspace(3, 1, 1000, 7) == 7, "first-anchor grid is capped at the max columns as a best effort");
    expect(gridColumnsToIncludeWorkspace(5, 1, 4, 7) == 5, "first-anchor grid never shrinks below the configured columns");
    expect(HyprexpoConfig::SHOW_PINNED_WINDOWS_DEFAULT == 0, "pinned windows are hidden from previews by default");
    expect(std::string{HyprexpoConfig::NUMBER_KEY_MODE_DEFAULT} == "workspace", "raw number keys keep selecting workspace IDs by default");
    expect(numberKeyModeFromString("workspace") == ENumberKeyMode::Workspace, "workspace number-key mode parses");
    expect(numberKeyModeFromString(" INDEX ") == ENumberKeyMode::Index, "index number-key mode is case-insensitive and trimmed");
    expect(numberKeyModeFromString("passthrough") == ENumberKeyMode::Passthrough, "passthrough number-key mode parses");
    expect(numberKeyModeFromString("invalid") == ENumberKeyMode::Workspace, "invalid number-key mode safely preserves the default");
    expect(HyprexpoConfig::DRAG_DROP_ENABLE_DEFAULT == 1, "drag and drop is enabled by default");

    const auto boundedGapFill = expandDynamicWorkspaceIDs({2, 4}, true, 64);
    expect(boundedGapFill.has_value(), "bounded fill_gaps range is accepted");
    expect(boundedGapFill == std::optional<std::vector<int64_t>>{{2, 3, 4}}, "bounded fill_gaps range expands missing IDs");

    const auto distantGapFill = expandDynamicWorkspaceIDs({1, 5000}, true, 64);
    expect(!distantGapFill.has_value(), "distant fill_gaps range is rejected before allocation");

    const auto extremeGapFill = expandDynamicWorkspaceIDs({std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()}, true, 64);
    expect(!extremeGapFill.has_value(), "fill_gaps span check cannot overflow");

    const auto sparseWithoutFill = expandDynamicWorkspaceIDs({1, 5000}, false, 64);
    expect(sparseWithoutFill == std::optional<std::vector<int64_t>>{{1, 5000}}, "disabled fill_gaps preserves sparse workspace IDs");

    expect(!shouldShowWorkspaceLabel(false, "always", true, true, true), "modern label_enable disables labels in dynamic mode");
    expect(!shouldShowWorkspaceLabel(true, "never", true, true, true), "modern label_show never hides labels in dynamic mode");
    expect(shouldShowWorkspaceLabel(true, "hover", true, false, false), "modern label_show hover displays the hovered label");
    expect(!shouldShowWorkspaceLabel(true, "hover", false, true, true), "modern label_show hover does not fall through to focus or current");

    expect(resolveBorderSpec("rgb(010203)", "0xffaabbcc") == "rgb(010203)", "modern border spec takes precedence over legacy color");
    expect(resolveBorderSpec("", "0xffaabbcc") == "0xffaabbcc", "legacy border color is used only when the modern spec is empty");

    expect(resolveLabelPosition("center", false, "top_right", false) == "center", "modern position default wins when neither key is explicit");
    expect(resolveLabelPosition("center", false, " top_right ", true) == "top_right", "explicit legacy position overrides an implicit modern default");
    expect(resolveLabelPosition(" bottom-left ", true, "top_right", true) == "bottom-left", "explicit modern position wins when both keys are explicit");

    expect(resolveLabelFontSize(16, false, 72, false) == 16, "modern size default wins when neither key is explicit");
    expect(resolveLabelFontSize(16, false, 72, true) == 36, "explicit legacy badge size converts to the historical font size");
    expect(resolveLabelFontSize(24, true, 72, true) == 24, "explicit modern font size wins when both keys are explicit");
    expect(resolveLabelFontSize(0, true, 72, true) == 8, "explicit modern font size is clamped instead of falling back to legacy");

    expect(tileIndexFromPoint(0, 0, 300, 300, 3) == 0, "legacy tile index top-left");
    expect(tileIndexFromPoint(299, 299, 300, 300, 3) == 8, "legacy tile index bottom-right inside");
    expect(tileIndexFromPoint(300, 300, 300, 300, 3) == 8, "legacy tile index clamps monitor edge");
    expect(tileIndexFromPoint(10, 10, 0, 300, 3) == -1, "legacy tile index rejects invalid width");
    expect(numberKeyToVisibleIndex(1) == 0, "number key 1 selects the first visible tile");
    expect(numberKeyToVisibleIndex(2) == 1, "number key 2 selects the second visible tile");
    expect(numberKeyToVisibleIndex(9) == 8, "number key 9 selects the ninth visible tile");
    expect(numberKeyToVisibleIndex(0) == 9, "number key 0 selects the tenth visible tile");
    expect(numberKeyToVisibleIndex(10) == -1, "out-of-range number is not a visible tile index");

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

    // Regression: a window at least as large as the target tile makes proxyW/proxyH clamp to
    // exactly tile.w/tile.h, so `tile.x + tile.w - proxyW` -- algebraically just tile.x -- is
    // catastrophic cancellation and can round an ULP below tile.x. Feeding that to clamp() as the
    // upper bound is UB and aborts under libstdc++ assertions, killing the compositor mid-render.
    //
    // 0.1 and 1200.5 are chosen because (0.1 + 1200.5) - 1200.5 < 0.1 in IEEE-754. Whether the
    // cancellation rounds down at all depends on the magnitude of the tile origin, which is why
    // dropping onto a tile in one grid row could crash while an adjacent tile was fine. Without
    // the fix this call does not return -- it aborts the test binary.
    SDropIntentInput tileFillingDropInput{
        .targetValid     = true,
        .pointerLocal    = {600, 600},
        .targetTileLocal = {0.1, 0.1, 1200.5, 1200.5},
        .workspaceSize   = {1200, 1200},
        .windowSize      = {2400, 2400}, // larger than the workspace, so the proxy fills the tile
        .grabOffset      = {0, 0},
    };
    const auto tileFillingDrop = computeDropIntentGeometry(tileFillingDropInput);
    expect(tileFillingDrop.valid, "drop intent stays valid when the window fills the tile");
    expect(near(tileFillingDrop.targetProxyLocal.w, tileFillingDropInput.targetTileLocal.w) && near(tileFillingDrop.targetProxyLocal.h, tileFillingDropInput.targetTileLocal.h),
           "a tile-filling proxy is capped to the tile size");
    expect(tileFillingDrop.targetProxyLocal.x >= tileFillingDropInput.targetTileLocal.x && tileFillingDrop.targetProxyLocal.y >= tileFillingDropInput.targetTileLocal.y,
           "a tile-filling proxy never starts outside the tile");

    dropInput.targetValid = false;
    expect(!computeDropIntentGeometry(dropInput).valid, "drop intent rejects invalid target");
    dropInput.targetValid   = true;
    dropInput.windowSize.w = 0;
    expect(!computeDropIntentGeometry(dropInput).valid, "drop intent rejects invalid window size");

    const char*       rawConfigString = "vertical";
    const void*       rawReply        = &rawConfigString;
    expect(decodeConfigString(rawReply, false, "up") == "vertical", "a const char* config reply is decoded, not treated as a type mismatch");

    std::string       stdConfigString = "horizontal";
    std::string*      stdConfigPtr    = &stdConfigString;
    expect(decodeConfigString(&stdConfigPtr, true, "up") == "horizontal", "a std::string config reply is still decoded");

    const char*       nullConfigString = nullptr;
    expect(decodeConfigString(&nullConfigString, false, "up") == "up", "a null inner pointer falls back to the default");
    expect(decodeConfigString(nullptr, false, "up") == "up", "an absent config value falls back to the default");

    const auto gestureDisabled = evaluateGestureSync({.fingers = 0, .direction = "up", .directionValid = true});
    expect(!gestureDisabled.registerGesture, "gesture_fingers = 0 registers nothing");
    expect(gestureDisabled.error.empty(), "gesture_fingers = 0 is opt-out, not a misconfiguration");

    const auto gestureEnabled = evaluateGestureSync({.fingers = 3, .direction = "vertical", .directionValid = true});
    expect(gestureEnabled.registerGesture && gestureEnabled.error.empty(), "a valid finger count and direction registers the gesture");

    for (const int fingers : {-1, 1, 10}) {
        const auto rejected = evaluateGestureSync({.fingers = fingers, .direction = "up", .directionValid = true});
        expect(!rejected.registerGesture, "gesture_fingers " + std::to_string(fingers) + " registers nothing");
        expect(rejected.error.find(std::to_string(fingers)) != std::string::npos, "gesture_fingers " + std::to_string(fingers) + " is reported with the offending value");
    }

    const auto badDirection = evaluateGestureSync({.fingers = 3, .direction = "sideways", .directionValid = false});
    expect(!badDirection.registerGesture, "an unknown gesture_direction registers nothing");
    expect(badDirection.error.find("sideways") != std::string::npos, "an unknown gesture_direction is reported with the offending value");

    expect(fallbackTokenForVisibleIndex(0) == "1", "fallback token first workspace");
    expect(fallbackTokenForVisibleIndex(9) == "0", "fallback token tenth workspace");
    expect(fallbackTokenForVisibleIndex(10) == "a", "fallback token alpha start");
    expect(fallbackTokenToVisibleIndex("A") == 10, "fallback token accepts uppercase alpha");
    expect(fallbackTokenToVisibleIndex("zz") == -1, "fallback token rejects multi-character fallback");

    SColorRGBA color;
    expect(parseHexRGBA8("33ccffee", color), "parse rgba hex");
    expect(near(color.r, 0x33 / 255.0) && near(color.a, 0xee / 255.0), "rgba channels are in rrggbbaa order");
    expect(parseSolidColorSpec("rgb(66ccff)", color), "parse rgb solid color");
    expect(near(color.r, 0x66 / 255.0) && near(color.a, 1.0), "rgb solid color is opaque");
    expect(parseSolidColorSpec("0x8066ccff", color), "parse argb solid color");
    expect(near(color.a, 0x80 / 255.0) && near(color.r, 0x66 / 255.0), "argb solid channel order");
    expect(!parseSolidColorSpec("rgb(nothex)", color), "invalid rgb solid rejected");

    const auto gradient = parseGradientSpec("rgba(33ccffee) rgba(00ff99ee) 45deg");
    expect(gradient.valid, "gradient parses two rgba colors");
    expect(near(gradient.angleDeg, 45.0), "gradient angle parses");
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

    checkGeometryForMonitor(makeSize(1600, 900));
    checkGeometryForMonitor(makeSize(900, 1600));
    checkGeometryForMonitor(makeSize(2560, 1080));

    if (failures != 0)
        return 1;

    std::cout << "HyprexpoLogicTests passed\n";
    return 0;
}
