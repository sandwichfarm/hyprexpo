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

bool containsKey(const std::vector<uint64_t>& keys, uint64_t key) {
    return std::find(keys.begin(), keys.end(), key) != keys.end();
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

    // --- global multi-monitor geometry ---
    {
        const SRect source{100, 100, 80, 80};
        const std::vector<SGlobalTile> candidates{
            {.overviewKey = 2, .tileIndex = 0, .overviewGlobal = {240, 0, 200, 300}, .tileGlobal = {240, 100, 60, 60}},
            {.overviewKey = 2, .tileIndex = 1, .overviewGlobal = {240, 0, 200, 300}, .tileGlobal = {320, 100, 60, 60}},
            {.overviewKey = 3, .tileIndex = 0, .overviewGlobal = {-200, 40, 180, 260}, .tileGlobal = {-100, 110, 50, 50}},
            {.overviewKey = 4, .tileIndex = 0, .overviewGlobal = {20, -240, 220, 180}, .tileGlobal = {110, -100, 50, 40}},
            {.overviewKey = 5, .tileIndex = 0, .overviewGlobal = {0, 260, 280, 220}, .tileGlobal = {115, 280, 70, 50}},
        };

        const auto RIGHT = selectDirectionalTile(source, EDirection::Right, candidates);
        expect(RIGHT && RIGHT->overviewKey == 2 && RIGHT->tileIndex == 0, "right selects nearest forward boundary tile");
        const auto LEFT = selectDirectionalTile(source, EDirection::Left, candidates);
        expect(LEFT && LEFT->overviewKey == 3, "left selects a negative-coordinate monitor tile");
        const auto UP = selectDirectionalTile(source, EDirection::Up, candidates);
        expect(UP && UP->overviewKey == 4, "up selects a stacked monitor tile");
        const auto DOWN = selectDirectionalTile(source, EDirection::Down, candidates);
        expect(DOWN && DOWN->overviewKey == 5, "down selects a gapped monitor tile");

        const std::vector<SGlobalTile> ranked{
            {.overviewKey = 10, .tileIndex = 0, .overviewGlobal = {200, 0, 300, 300}, .tileGlobal = {220, 260, 40, 40}},
            {.overviewKey = 11, .tileIndex = 0, .overviewGlobal = {200, 0, 300, 300}, .tileGlobal = {220, 130, 40, 40}},
            {.overviewKey = 12, .tileIndex = 0, .overviewGlobal = {200, 0, 300, 300}, .tileGlobal = {240, 110, 40, 40}},
        };
        const auto PERPENDICULAR = selectDirectionalTile(source, EDirection::Right, ranked);
        expect(PERPENDICULAR && PERPENDICULAR->overviewKey == 11, "perpendicular interval distance ranks before center distance");

        const std::vector<SGlobalTile> stableTie{
            {.overviewKey = 20, .tileIndex = 3, .overviewGlobal = {200, 0, 300, 300}, .tileGlobal = {220, 100, 40, 40}},
            {.overviewKey = 21, .tileIndex = 1, .overviewGlobal = {200, 0, 300, 300}, .tileGlobal = {220, 100, 40, 40}},
        };
        const auto TIE = selectDirectionalTile(source, EDirection::Right, stableTie);
        expect(TIE && TIE->overviewKey == 20 && TIE->tileIndex == 3, "directional ties retain stable input order");

        const std::vector<SGlobalTile> partialRow{
            {.overviewKey = 30, .tileIndex = 3, .overviewGlobal = {220, 0, 400, 300}, .tileGlobal = {220, 20, 80, 80}},
            {.overviewKey = 30, .tileIndex = 4, .overviewGlobal = {220, 0, 400, 300}, .tileGlobal = {220, 120, 80, 80}},
        };
        const auto PARTIAL = selectDirectionalTile(source, EDirection::Right, partialRow);
        expect(PARTIAL && PARTIAL->tileIndex == 4, "partial-row candidate nearest the source interval wins");

        const std::vector<SGlobalTile> invalid{
            {.overviewKey = 0, .tileIndex = 0, .overviewGlobal = {200, 0, 100, 100}, .tileGlobal = {220, 100, 40, 40}},
            {.overviewKey = 2, .tileIndex = -1, .overviewGlobal = {200, 0, 100, 100}, .tileGlobal = {220, 100, 40, 40}},
            {.overviewKey = 2, .tileIndex = 0, .overviewGlobal = {200, 0, 100, 100}, .tileGlobal = {220, 100, 0, 40}},
            {.overviewKey = 2, .tileIndex = 1, .overviewGlobal = {0, 0, 100, 100}, .tileGlobal = {20, 100, 40, 40}},
        };
        expect(!selectDirectionalTile(source, EDirection::Right, invalid), "invalid and wrong-half-plane candidates are ignored");
    }

    // --- global overview/tile hit testing ---
    {
        const std::vector<SGlobalTile> tiles{
            {.overviewKey = 1, .tileIndex = 0, .overviewGlobal = {-1920, 0, 1920, 1080}, .tileGlobal = {-1900, 20, 900, 500}},
            {.overviewKey = 2, .tileIndex = 4, .overviewGlobal = {100, 200, 1280, 720}, .tileGlobal = {140, 240, 500, 200}},
        };
        const auto FIRST = hitTestGlobalTile({-1900, 20}, tiles);
        expect(FIRST && FIRST->overviewKey == 1 && FIRST->tileIndex == 0, "global hit includes the top-left tile edge");
        expect(FIRST && near(FIRST->pointLocal.x, 20) && near(FIRST->pointLocal.y, 20), "hit result converts to first overview-local coordinates");
        const auto SECOND = hitTestGlobalTile({639.999, 439.999}, tiles);
        expect(SECOND && SECOND->overviewKey == 2 && SECOND->tileIndex == 4, "global hit resolves a differently sized target overview");
        expect(SECOND && near(SECOND->pointLocal.x, 539.999) && near(SECOND->pointLocal.y, 239.999), "hit result converts to target-local coordinates");
        expect(!hitTestGlobalTile({640, 440}, tiles), "global hit excludes exact bottom-right tile edges");
        expect(!hitTestGlobalTile({50, 100}, tiles), "global hit rejects monitor gaps and points outside every tile");
    }

    // --- executable shared drag coordinator ---
    {
        SOverviewDragState state;
        const std::vector<uint64_t> LIVE{1, 2, 3};

        auto step = transitionOverviewDrag(state, {.type = EOverviewDragEventType::Press, .monitorKey = 1, .tileIndex = 2, .windowKey = 77}, LIVE);
        expect(step.accepted && step.next.active && step.next.sourceMonitorKey == 1, "press claims one live source owner");
        state = step.next;

        const auto SECOND_PRESS = transitionOverviewDrag(state, {.type = EOverviewDragEventType::Press, .monitorKey = 2, .tileIndex = 0, .windowKey = 88}, LIVE);
        expect(!SECOND_PRESS.accepted && SECOND_PRESS.next.sourceMonitorKey == 1, "a concurrent second press cannot steal drag ownership");

        step = transitionOverviewDrag(state, {.type = EOverviewDragEventType::Move}, LIVE);
        expect(step.accepted && step.next.moved, "move crosses the drag threshold once");
        state = step.next;
        step = transitionOverviewDrag(state, {.type = EOverviewDragEventType::Target, .monitorKey = 2, .tileIndex = 4}, LIVE);
        expect(step.accepted && step.next.targetMonitorKey == 2, "target A becomes current");
        state = step.next;
        step = transitionOverviewDrag(state, {.type = EOverviewDragEventType::Target, .monitorKey = 3, .tileIndex = 1}, LIVE);
        expect(step.accepted && step.next.targetMonitorKey == 3, "target B replaces target A");
        expect(containsKey(step.next.affectedMonitorKeys, 1) && containsKey(step.next.affectedMonitorKeys, 2) && containsKey(step.next.affectedMonitorKeys, 3), "source and every visited target remain affected");
        state = step.next;

        step = transitionOverviewDrag(state, {.type = EOverviewDragEventType::Release}, LIVE);
        expect(step.drop && step.drop->sourceMonitorKey == 1 && step.drop->targetMonitorKey == 3 && step.drop->windowKey == 77, "release emits one validated source-to-current-target drop");
        expect(step.cleanup && !step.next.active, "release returns cleanup and leaves the coordinator idle");
        expect(step.cleanupMonitorKeys.size() == 3, "release cleanup damages each affected monitor once");
        state = step.next;
        const auto DUPLICATE_RELEASE = transitionOverviewDrag(state, {.type = EOverviewDragEventType::Release}, LIVE);
        expect(!DUPLICATE_RELEASE.drop && !DUPLICATE_RELEASE.cleanup && !DUPLICATE_RELEASE.next.active, "duplicate release is idempotent and emits no second drop");

        const auto STALE_PRESS = transitionOverviewDrag({}, {.type = EOverviewDragEventType::Press, .monitorKey = 9, .tileIndex = 0, .windowKey = 1}, LIVE);
        expect(!STALE_PRESS.accepted && !STALE_PRESS.next.active, "press rejects a stale source monitor key");
        const auto VALID_PRESS = transitionOverviewDrag({}, {.type = EOverviewDragEventType::Press, .monitorKey = 1, .tileIndex = 0, .windowKey = 1}, LIVE);
        const auto STALE_TARGET = transitionOverviewDrag(VALID_PRESS.next, {.type = EOverviewDragEventType::Target, .monitorKey = 9, .tileIndex = 0}, LIVE);
        expect(!STALE_TARGET.accepted && !STALE_TARGET.next.targetMonitorKey, "target change rejects a stale target monitor key");

        auto noTarget = transitionOverviewDrag(VALID_PRESS.next, {.type = EOverviewDragEventType::Move}, LIVE).next;
        const auto NO_TARGET_RELEASE = transitionOverviewDrag(noTarget, {.type = EOverviewDragEventType::Release}, LIVE);
        expect(!NO_TARGET_RELEASE.drop && NO_TARGET_RELEASE.cleanup && !NO_TARGET_RELEASE.next.active, "release without a target cleans up without a drop");

        auto targetState = transitionOverviewDrag(noTarget, {.type = EOverviewDragEventType::Target, .monitorKey = 2, .tileIndex = 1}, LIVE).next;
        const auto sameTile = transitionOverviewDrag(
            transitionOverviewDrag(VALID_PRESS.next, {.type = EOverviewDragEventType::Move}, LIVE).next,
            {.type = EOverviewDragEventType::Target, .monitorKey = 1, .tileIndex = 0}, LIVE).next;
        const auto SAME_TILE_RELEASE = transitionOverviewDrag(sameTile, {.type = EOverviewDragEventType::Release}, LIVE);
        expect(!SAME_TILE_RELEASE.drop && SAME_TILE_RELEASE.cleanup, "same-tile release is consumed without emitting a drop");

        const auto CLEARED_TARGET = transitionOverviewDrag(targetState, {.type = EOverviewDragEventType::Target}, LIVE);
        expect(CLEARED_TARGET.accepted && !CLEARED_TARGET.next.targetMonitorKey, "leaving every overview clears the current target without forgetting affected monitors");
        const auto GAP_RELEASE = transitionOverviewDrag(CLEARED_TARGET.next, {.type = EOverviewDragEventType::Release}, LIVE);
        expect(!GAP_RELEASE.drop && GAP_RELEASE.cleanup && GAP_RELEASE.cleanupMonitorKeys.size() == 2, "gap release cleans source and former target without a drop");

        auto targetBState = transitionOverviewDrag(targetState, {.type = EOverviewDragEventType::Target, .monitorKey = 3, .tileIndex = 0}, LIVE).next;
        const auto FORMER_TARGET_DESTROY = transitionOverviewDrag(targetBState, {.type = EOverviewDragEventType::MonitorDestroyed, .monitorKey = 2}, {1, 3});
        expect(FORMER_TARGET_DESTROY.cleanup && !FORMER_TARGET_DESTROY.next.active, "destroying a former target invalidates and cleans the whole session");

        const auto UNKNOWN_LOST_SOURCE = transitionOverviewDrag(targetState, {.type = EOverviewDragEventType::MonitorDestroyed}, {2, 3});
        expect(UNKNOWN_LOST_SOURCE.cleanup && !UNKNOWN_LOST_SOURCE.next.active,
               "an expired source resets the session even when the destruction key is unavailable");
        const auto UNKNOWN_LOST_TARGET = transitionOverviewDrag(targetState, {.type = EOverviewDragEventType::MonitorDestroyed, .monitorKey = 99}, {1, 3});
        expect(UNKNOWN_LOST_TARGET.cleanup && !UNKNOWN_LOST_TARGET.next.active,
               "an expired target resets the session before an unknown destruction key is rejected");
        const auto UNKNOWN_ALL_LIVE = transitionOverviewDrag(targetState, {.type = EOverviewDragEventType::MonitorDestroyed, .monitorKey = 99}, LIVE);
        expect(!UNKNOWN_ALL_LIVE.cleanup && UNKNOWN_ALL_LIVE.next.active,
               "an unrelated destruction key leaves a fully live session intact");

        const auto LOST_TARGET_RELEASE = transitionOverviewDrag(targetState, {.type = EOverviewDragEventType::Release}, {1, 3});
        expect(!LOST_TARGET_RELEASE.drop && LOST_TARGET_RELEASE.cleanup, "release rejects a target that disappeared without a destruction callback");

        const auto TARGET_DESTROY = transitionOverviewDrag(targetState, {.type = EOverviewDragEventType::MonitorDestroyed, .monitorKey = 2}, {1, 3});
        expect(TARGET_DESTROY.cleanup && !TARGET_DESTROY.drop && !TARGET_DESTROY.next.active, "target destruction cancels and cleans the session");
        expect(containsKey(TARGET_DESTROY.cleanupMonitorKeys, 1) && containsKey(TARGET_DESTROY.cleanupMonitorKeys, 2), "target destruction retains source and destroyed-target cleanup keys");

        const auto SOURCE_DESTROY = transitionOverviewDrag(targetState, {.type = EOverviewDragEventType::MonitorDestroyed, .monitorKey = 1}, {2, 3});
        expect(SOURCE_DESTROY.cleanup && !SOURCE_DESTROY.next.active, "source destruction cancels and cleans the session");
        const auto CANCEL = transitionOverviewDrag(targetState, {.type = EOverviewDragEventType::Cancel}, LIVE);
        expect(CANCEL.cleanup && !CANCEL.next.active && !CANCEL.drop, "cancel cleans without emitting a drop");
        const auto ALL_CLOSE = transitionOverviewDrag(targetState, {.type = EOverviewDragEventType::AllClose}, {});
        expect(ALL_CLOSE.cleanup && !ALL_CLOSE.next.active && ALL_CLOSE.cleanupMonitorKeys.size() == 2, "all-close cleans and de-duplicates affected monitors");
        const auto IDLE_CANCEL = transitionOverviewDrag({}, {.type = EOverviewDragEventType::Cancel}, LIVE);
        expect(!IDLE_CANCEL.cleanup && !IDLE_CANCEL.next.active, "repeated cleanup while idle is harmless");
    }

    // --- "all monitors" qualifier on expo dispatcher args ---
    {
        const auto BARE = Hyprexpo::parseExpoCommand("all");
        expect(BARE.allMonitors, "bare all targets every monitor");
        expect(BARE.command == "toggle", "bare all means toggle all");

        const auto TOGGLE = Hyprexpo::parseExpoCommand("toggle all");
        expect(TOGGLE.allMonitors, "toggle all targets every monitor");
        expect(TOGGLE.command == "toggle", "toggle all keeps the toggle command");

        const auto ON = Hyprexpo::parseExpoCommand("on all");
        expect(ON.allMonitors, "on all targets every monitor");
        expect(ON.command == "on", "on all keeps the on command");

        const auto PLAIN = Hyprexpo::parseExpoCommand("toggle");
        expect(!PLAIN.allMonitors, "plain toggle stays single-monitor");
        expect(PLAIN.command == "toggle", "plain toggle is unchanged");

        const auto EMPTY = Hyprexpo::parseExpoCommand("");
        expect(!EMPTY.allMonitors, "empty arg stays single-monitor");
        expect(EMPTY.command.empty(), "empty arg stays empty");

        const auto PADDED = Hyprexpo::parseExpoCommand("  toggle   all  ");
        expect(PADDED.allMonitors, "surrounding whitespace does not hide the qualifier");
        expect(PADDED.command == "toggle", "whitespace is trimmed off the command");

        // "all" only counts as a qualifier on its own or as a trailing word.
        const auto SUBSTRING = Hyprexpo::parseExpoCommand("install");
        expect(!SUBSTRING.allMonitors, "a command merely ending in the letters all is not a qualifier");
        expect(SUBSTRING.command == "install", "non-qualifier command is unchanged");
    }

    if (failures != 0)
        return 1;

    std::cout << "HyprexpoLogicTests passed\n";
    return 0;
}
