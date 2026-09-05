#include "../HyprexpoLogic.hpp"
#include "../HyprexpoConfig.hpp"
#include "../ScrollingOverviewLogic.hpp"
#include "../ScrollingInputState.hpp"
#include "../ScrollingMutationTransaction.hpp"
#include "../ScrollingRequestId.hpp"

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

void checkFixedGridGeometryForMonitor(const Hyprexpo::SSize& total) {
    constexpr double gap = 24.0;
    for (const auto shape : std::vector<Hyprexpo::SGridShape>{{5, 2}, {2, 5}, {1, 5}, {5, 1}, {3, 2}, {2, 3}, {7, 7}}) {
        const int count = shape.cols * shape.rows;
        const auto layouts = layoutsFor(count, shape, total, gap, false);
        const auto& first = layouts.front().box;
        const auto& last = layouts.back().box;
        expect(near(first.x, total.w - last.x - last.w) && near(first.y, total.h - last.y - last.h),
               "fixed rectangle is letterboxed symmetrically");
        const double zoom = std::max(shape.cols, shape.rows);
        for (int i = 0; i < count; ++i) {
            const auto& layout = layouts[i];
            const auto& box = layout.box;
            expect(layout.row == i / shape.cols && layout.col == i % shape.cols, "fixed rectangle keeps row-major tile indices");
            expect(box.x >= 0.0 && box.y >= 0.0 && box.x + box.w <= total.w + 0.001 && box.y + box.h <= total.h + 0.001,
                   "every fixed tile fits the monitor");
            expect(near(box.w / box.h, total.w / total.h), "fixed tiles preserve workspace aspect ratio");
            expect(Hyprexpo::tileIndexAtPoint(box.x + box.w / 2, box.y + box.h / 2, count, shape, total, gap, false) == i,
                   "every rectangular tile is hit-testable including the last slot");
            const auto zoomed = Hyprexpo::computeTileLayout(i, count, shape, {total.w * zoom, total.h * zoom}, 0.0, false);
            expect(near(zoomed.box.w, total.w) && near(zoomed.box.h, total.h), "rectangular zoom endpoints retain full workspace size");
        }
        if (shape.cols > 1)
            expect(Hyprexpo::tileIndexAtPoint(first.x + first.w + gap / 2, first.y + first.h / 2, count, shape, total, gap, false) == -1,
                   "fixed-grid column gaps are not selectable");
        if (shape.rows > 1)
            expect(Hyprexpo::tileIndexAtPoint(first.x + first.w / 2, first.y + first.h + gap / 2, count, shape, total, gap, false) == -1,
                   "fixed-grid row gaps are not selectable");
        if (first.x > 0.0)
            expect(Hyprexpo::tileIndexAtPoint(first.x / 2, first.y + first.h / 2, count, shape, total, gap, false) == -1,
                   "horizontal letterboxing is not a workspace target");
        if (first.y > 0.0)
            expect(Hyprexpo::tileIndexAtPoint(first.x + first.w / 2, first.y / 2, count, shape, total, gap, false) == -1,
                   "vertical letterboxing is not a workspace target");
    }
}

Hyprexpo::Scrolling::STapeSpec scrollingTape(Hyprexpo::Scrolling::EDirection direction) {
    using namespace Hyprexpo::Scrolling;
    return {
        .direction = direction,
        .columns = {
            {.token = 10, .extent = 300.0, .targets = {{.token = 101, .proportion = 1.0}}},
            {.token = 20, .extent = 200.0, .targets = {{.token = 201, .proportion = 1.0}, {.token = 202, .proportion = 3.0}}},
            {.token = 30, .extent = 400.0, .targets = {{.token = 301, .proportion = 1.0}}},
        },
    };
}

Hyprexpo::Scrolling::SScene scrollingScene() {
    using namespace Hyprexpo::Scrolling;
    return buildScene(
        {
            {.workspaceID = 1, .kind = EWorkspaceKind::Scrolling, .tape = scrollingTape(EDirection::Right)},
            {.workspaceID = 2, .kind = EWorkspaceKind::Empty, .tape = {}},
            {.workspaceID = 3, .kind = EWorkspaceKind::Mixed, .tape = {}},
        },
        2,
        {.viewportWidth = 1000.0, .viewportHeight = 500.0, .rowHeight = 300.0, .rowGap = 40.0, .columnGap = 10.0, .terminalWorkspaceID = 4});
}

void checkScrollingTapeDirections() {
    using namespace Hyprexpo::Scrolling;

    for (const auto direction : {EDirection::Right, EDirection::Left, EDirection::Down, EDirection::Up}) {
        const auto layout = layoutTape(scrollingTape(direction), {25.0, 50.0}, 600.0, 10.0);
        expect(layout.valid, "scrolling tape accepts complete positive topology");
        expect(layout.targets.size() == 4, "scrolling tape retains every offscreen target");
        expect(layout.targets[0].token == 101 && layout.targets[1].token == 201 && layout.targets[2].token == 202 && layout.targets[3].token == 301,
               "scrolling tape output preserves native target identity/order");
        expect(layout.targets[1].nativeColumnIndex == 1 && layout.targets[2].nativeRowIndex == 1,
               "scrolling tape retains native column/row indices");

        for (size_t i = 0; i < layout.targets.size(); ++i) {
            const auto& box = layout.targets[i].box;
            expect(box.w > 0.0 && box.h > 0.0 && std::isfinite(box.x) && std::isfinite(box.y), "scrolling target boxes are finite and positive");
            for (size_t j = i + 1; j < layout.targets.size(); ++j) {
                const auto& other = layout.targets[j].box;
                const bool overlap = box.x < other.x + other.w && box.x + box.w > other.x && box.y < other.y + other.h && box.y + box.h > other.y;
                expect(!overlap, "scrolling target boxes never overlap");
            }
        }

        const auto& firstColumn = layout.targets[0].box;
        const auto& lastColumn  = layout.targets[3].box;
        if (direction == EDirection::Right)
            expect(firstColumn.x < lastColumn.x, "right direction places native order left-to-right");
        else if (direction == EDirection::Left)
            expect(firstColumn.x > lastColumn.x, "left direction reverses presentation without reversing identity order");
        else if (direction == EDirection::Down)
            expect(firstColumn.y < lastColumn.y, "down direction places native order top-to-bottom");
        else
            expect(firstColumn.y > lastColumn.y, "up direction reverses presentation without reversing identity order");

        const auto& upper = layout.targets[1].box;
        const auto& lower = layout.targets[2].box;
        const double firstShare = direction == EDirection::Right || direction == EDirection::Left ? upper.h / (upper.h + lower.h) : upper.w / (upper.w + lower.w);
        expect(near(firstShare, 0.25), "target row proportions are preserved on the cross axis");
    }

    auto invalid = scrollingTape(EDirection::Right);
    invalid.columns[0].extent = std::numeric_limits<double>::infinity();
    expect(!layoutTape(invalid, {}, 600.0, 10.0).valid, "scrolling tape rejects non-finite column extent");
    invalid = scrollingTape(EDirection::Right);
    invalid.columns[1].targets[0].proportion = 0.0;
    expect(!layoutTape(invalid, {}, 600.0, 10.0).valid, "scrolling tape rejects non-positive target proportion");
}

void checkScrollingSceneAndInputMath() {
    using namespace Hyprexpo::Scrolling;

    const auto scene = scrollingScene();
    expect(scene.valid && scene.workspaces.size() == 4, "scene includes scrolling, empty, mixed, and terminal rows");
    expect(scene.targets.size() == 4, "scene retains the full native tape");

    const double initial = initialPan(scene, 2, 500.0);
    expect(near(initial, 240.0), "active workspace row is initially centered");
    expect(near(panBy(scene, initial, -10000.0, 500.0), 0.0), "mouse-axis pan clamps at the first row");
    expect(near(panBy(scene, initial, 10000.0, 500.0), scene.contentHeight - 500.0), "touch pan clamps at the terminal row");

    const auto& target = scene.targets.front();
    const auto targetHit = hitTest(scene, {target.box.x + target.box.w / 2.0, target.box.y + target.box.h / 2.0 - initial}, initial);
    expect(targetHit.kind == EHitKind::Target && targetHit.targetToken == target.token, "hit test returns the exact target");
    const auto emptyHit = hitTest(scene, {500.0, scene.workspaces[1].box.y + 20.0 - initial}, initial);
    expect(emptyHit.kind == EHitKind::EmptyWorkspace && emptyHit.workspaceID == 2, "hit test returns an ordinary empty row");
    const auto mixedHit = hitTest(scene, {500.0, scene.workspaces[2].box.y + 20.0 - initial}, initial);
    expect(mixedHit.kind == EHitKind::MixedWorkspace && mixedHit.workspaceID == 3, "hit test returns an ordinary mixed row");
    const auto terminalHit = hitTest(scene, {500.0, scene.workspaces[3].box.y + 20.0 - initial}, initial);
    expect(terminalHit.kind == EHitKind::TerminalWorkspace && terminalHit.workspaceID == 4, "hit test returns the terminal next-empty row");
    expect(hitTest(scene, {-1.0, 20.0}, initial).kind == EHitKind::Outside, "hit test rejects points outside all rows");

    const SFocusRef first{.kind = EHitKind::Target, .workspaceID = 1, .targetToken = 101};
    const auto right = moveFocus(scene, first, EFocusDirection::Right);
    expect(right.kind == EHitKind::Target && right.targetToken == 202, "spatial focus chooses the closest aligned target across columns deterministically");
    const auto down = moveFocus(scene, first, EFocusDirection::Down);
    expect(down.kind == EHitKind::EmptyWorkspace && down.workspaceID == 2, "spatial focus moves deterministically across rows");
    expect(moveFocus(scene, first, EFocusDirection::Left).targetToken == 101, "spatial focus stays put when no candidate exists");
}

void checkScrollingDropIntents() {
    using namespace Hyprexpo::Scrolling;

    const auto scene = scrollingScene();
    const auto& target = scene.targets[1];
    const auto& rowTarget = scene.targets[2];
    const SDropSource source{.workspaceID = 1, .columnIndex = 1, .rowIndex = 0, .sourceColumnWillDisappear = false};

    auto intent = resolveDrop(scene, source, {rowTarget.box.x + rowTarget.box.w / 2.0, rowTarget.box.y + rowTarget.box.h * 0.9}, 0.0);
    expect(intent.kind == EDropKind::ExistingColumn && intent.placement == EColumnPlacement::Existing && intent.rowIndex == 1,
           "drop center resolves same-column row reorder");

    intent = resolveDrop(scene, {.workspaceID = 1, .columnIndex = 0, .rowIndex = 0}, {target.box.x + 1.0, target.box.y + target.box.h / 2.0}, 0.0);
    expect(intent.kind == EDropKind::NewColumnBefore && intent.placement == EColumnPlacement::Before, "primary start edge creates a column before");
    intent = resolveDrop(scene, {.workspaceID = 1, .columnIndex = 0, .rowIndex = 0}, {target.box.x + target.box.w - 1.0, target.box.y + target.box.h / 2.0}, 0.0);
    expect(intent.kind == EDropKind::NewColumnAfter && intent.placement == EColumnPlacement::After, "primary end edge creates a column after");

    const auto emptyY = scene.workspaces[1].box.y + 20.0;
    intent = resolveDrop(scene, source, {500.0, emptyY}, 0.0);
    expect(intent.kind == EDropKind::CrossWorkspace && intent.workspaceID == 2, "empty scrolling destination resolves cross-workspace drop");
    intent = resolveDrop(scene, source, {500.0, scene.workspaces[2].box.y + 20.0}, 0.0);
    expect(intent.kind == EDropKind::MixedFallback && intent.workspaceID == 3, "mixed destination resolves fallback drop");
    intent = resolveDrop(scene, source, {500.0, scene.workspaces[3].box.y + 20.0}, 0.0);
    expect(intent.kind == EDropKind::TerminalWorkspace && intent.workspaceID == 4, "terminal row resolves next-empty workspace drop");
    expect(resolveDrop(scene, source, {-1.0, -1.0}, 0.0).kind == EDropKind::Invalid, "outside release is invalid/no-op");

    intent = resolveDrop(scene, source, {target.box.x + target.box.w / 2.0, target.box.y + target.box.h * 0.25}, 0.0);
    expect(intent.kind == EDropKind::NoOp, "same-column same-row release is an explicit no-op");
    expect(adjustDestinationColumnIndex(1, 4, true) == 3, "destination index adjusts after an earlier source column disappears");
    expect(adjustDestinationColumnIndex(4, 1, true) == 1, "destination index is stable when source follows destination");
}

void checkScrollingInputCoordinates() {
    using namespace Hyprexpo;
    using namespace Hyprexpo::Scrolling;

    const SMonitorGeometry monitor{
        .position = {-1920.0, 120.0},
        .logicalSize = {1280.0, 720.0},
        .pixelSize = {1920.0, 1080.0},
        .scale = 1.5,
        .transform = EOutputTransform::Normal,
    };
    const auto local = monitorLocalPoint({-1280.0, 480.0}, monitor);
    expect(local && near(local->x, 640.0) && near(local->y, 360.0), "global pointer converts once to monitor-local logical coordinates at fractional scale");
    expect(!monitorLocalPoint({-640.0, 480.0}, monitor), "monitor right edge is outside the half-open logical box");
    expect(!monitorLocalPoint({-1920.0, 840.0}, monitor), "monitor bottom edge is outside the half-open logical box");

    const auto touchNormal = touchToGlobalLogical({0.25, 0.75}, monitor);
    expect(touchNormal && near(touchNormal->x, -1600.0) && near(touchNormal->y, 660.0), "normal touch coordinates use logical monitor geometry rather than pixels");

    auto transformed = monitor;
    transformed.position = {100.0, -900.0};
    transformed.logicalSize = {600.0, 1000.0};
    transformed.pixelSize = {1200.0, 2000.0};
    transformed.scale = 2.0;
    transformed.transform = EOutputTransform::Rotate90;
    const auto touchRotated = touchToGlobalLogical({0.2, 0.3}, transformed);
    expect(touchRotated && near(touchRotated->x, 520.0) && near(touchRotated->y, -700.0), "rotated touch coordinates map through the output transform exactly once");
    transformed.transform = EOutputTransform::Flipped270;
    const auto touchFlipped = touchToGlobalLogical({0.2, 0.3}, transformed);
    expect(touchFlipped && near(touchFlipped->x, 520.0) && near(touchFlipped->y, -100.0), "flipped rotated touch mapping follows the calibrated transform matrix");
    expect(!touchToGlobalLogical({1.01, 0.5}, transformed), "touch coordinates outside normalized bounds are rejected");
}

void checkScrollingRequestIds() {
    using namespace Hyprexpo::Scrolling;

    expect(validRequestID("runtime.case-1_A"), "shared request ID grammar accepts dot, underscore, and dash");
    expect(parseInputSequence("runtime.case-1_A|mouse_move:1:2").valid, "input injection accepts the shared dotted request ID grammar");
    expect(!validRequestID(""), "shared request ID grammar rejects empty IDs");
    expect(!validRequestID(std::string(65, 'a')), "shared request ID grammar rejects IDs longer than 64 bytes");
    expect(!validRequestID("slash/not-allowed"), "shared request ID grammar rejects punctuation outside dot, underscore, and dash");
}

void checkScrollingOverviewTransition() {
    using namespace Hyprexpo;
    using namespace Hyprexpo::Scrolling;

    const SSize viewport{1000.0, 500.0};
    const SRect box{100.0, 50.0, 300.0, 200.0};
    const auto open = overviewTransition(1.0, viewport);
    const auto openBox = applyOverviewTransition(box, viewport, open);
    expect(near(open.progress, 1.0) && near(open.opacity, 1.0) && near(open.scale, 1.0), "completed scrolling overview transition is fully visible and unscaled");
    expect(near(openBox.x, box.x) && near(openBox.y, box.y) && near(openBox.w, box.w) && near(openBox.h, box.h), "completed transition preserves render geometry");

    const auto closed = overviewTransition(0.0, viewport);
    const auto closedBox = applyOverviewTransition(box, viewport, closed);
    expect(near(closed.opacity, 0.0) && closed.scale < 1.0 && closedBox.y > box.y, "closed transition is faded, inset, and visibly displaced");
    expect(near(transitionForSwipe(false, 50.0, 100.0), 0.5), "opening swipe advances transition progress");
    expect(near(transitionForSwipe(true, 50.0, 100.0), 0.5), "closing swipe reverses transition progress");
    expect(near(transitionForSwipe(false, 500.0, 100.0), 1.0) && near(transitionForSwipe(true, 500.0, 100.0), 0.0), "swipe transition progress clamps at both animation endpoints");
}

void checkScrollingMouseInputState() {
    using namespace Hyprexpo;
    using namespace Hyprexpo::Scrolling;

    const auto scene = scrollingScene();
    const SMonitorGeometry monitor{.position = {100.0, -50.0}, .logicalSize = {1000.0, 500.0}, .pixelSize = {1500.0, 750.0}, .scale = 1.5};
    SInputContext context{.scene = &scene, .monitor = monitor, .pan = 0.0, .viewportHeight = 500.0, .dragThreshold = 12.0};
    SInputState state;
    const auto& target = scene.targets[1];
    const SPoint targetGlobal{monitor.position.x + target.box.x + target.box.w / 2.0, monitor.position.y + target.box.y + target.box.h / 2.0};

    auto result = transitionInput(state, {.kind = EInputKind::MouseMove, .globalLogicalPoint = targetGlobal}, context);
    expect(!result.effects.consume && result.effects.hoverChanged && result.state.hover.targetToken == target.token, "idle mouse hover changes exact target without consuming cursor motion");
    state = result.state;
    result = transitionInput(state, {.kind = EInputKind::MouseMove, .globalLogicalPoint = {monitor.position.x - 1.0, monitor.position.y}}, context);
    expect(!result.effects.consume && result.effects.clearHover && result.state.hover.kind == EHitKind::Outside, "outside hover clears and passes through");
    state = result.state;

    result = transitionInput(state, {.kind = EInputKind::MouseButton, .globalLogicalPoint = targetGlobal, .button = 0x111, .pressed = true}, context);
    expect(result.effects.consume && result.state.mode == EInputMode::MousePressPending && result.state.pressed.targetToken == target.token,
           "primary mouse down owns one exact target");
    state = result.state;
    result = transitionInput(state, {.kind = EInputKind::MouseMove, .globalLogicalPoint = {targetGlobal.x + 6.0, targetGlobal.y + 6.0}}, context);
    expect(result.effects.consume && result.state.mode == EInputMode::MousePressPending && !result.effects.beginDrag, "under-threshold mouse motion stays click-pending");
    state = result.state;
    result = transitionInput(state, {.kind = EInputKind::MouseButton, .globalLogicalPoint = {targetGlobal.x + 6.0, targetGlobal.y + 6.0}, .button = 0x111, .pressed = false}, context);
    expect(result.effects.consume && result.effects.selection && result.effects.selection->targetToken == target.token && result.effects.resetOwnership,
           "under-threshold primary release selects exactly the pressed target once");
    state = result.state;
    expect(state.mode == EInputMode::Idle, "click release returns input ownership to idle");

    result = transitionInput(state, {.kind = EInputKind::MouseButton, .globalLogicalPoint = targetGlobal, .button = 0x110, .pressed = true}, context);
    expect(!result.effects.consume && result.state.mode == EInputMode::Idle, "non-primary mouse button passes through");
    result = transitionInput(state, {.kind = EInputKind::MouseButton, .globalLogicalPoint = {monitor.position.x - 2.0, monitor.position.y}, .button = 0x111, .pressed = true}, context);
    expect(!result.effects.consume && result.state.mode == EInputMode::Idle, "outside primary mouse button passes through");

    result = transitionInput(state, {.kind = EInputKind::MouseButton, .globalLogicalPoint = targetGlobal, .button = 0x111, .pressed = true}, context);
    state = result.state;
    result = transitionInput(state, {.kind = EInputKind::MouseMove, .globalLogicalPoint = {targetGlobal.x + 13.0, targetGlobal.y}}, context);
    expect(result.effects.consume && result.effects.beginDrag && result.state.mode == EInputMode::WindowDrag && result.effects.dropIntent.has_value(),
           "threshold crossing begins exactly one window drag and resolves an intent");
    state = result.state;
    const auto mixed = scene.workspaces[2];
    context.pan = 340.0;
    const SPoint mixedGlobal{monitor.position.x + 500.0, monitor.position.y + mixed.box.y + 20.0 - context.pan};
    result = transitionInput(state, {.kind = EInputKind::MouseMove, .globalLogicalPoint = mixedGlobal}, context);
    expect(result.effects.consume && result.effects.updateDrag && !result.effects.beginDrag && result.effects.dropIntent && result.effects.dropIntent->kind == EDropKind::MixedFallback,
           "owned drag motion updates one mixed-workspace pure intent");
    state = result.state;
    result = transitionInput(state, {.kind = EInputKind::MouseButton, .globalLogicalPoint = mixedGlobal, .button = 0x111, .pressed = false}, context);
    expect(result.effects.consume && result.effects.finishDrag && result.effects.dropIntent && result.effects.dropIntent->kind == EDropKind::MixedFallback,
           "drag release emits one finish effect and does not mutate topology");

    context.pan = 0.0;
    state = transitionInput({}, {.kind = EInputKind::MouseButton, .globalLogicalPoint = targetGlobal, .button = 0x111, .pressed = true}, context).state;
    state = transitionInput(state, {.kind = EInputKind::MouseMove, .globalLogicalPoint = {targetGlobal.x + 20.0, targetGlobal.y}}, context).state;
    result = transitionInput(state, {.kind = EInputKind::MouseButton, .globalLogicalPoint = {monitor.position.x - 10.0, monitor.position.y}, .button = 0x111, .pressed = false}, context);
    expect(result.effects.consume && result.effects.cancelDrag && !result.effects.finishDrag, "outside drag release cancels without a drop");

    result = transitionInput({}, {.kind = EInputKind::MouseAxis, .globalLogicalPoint = targetGlobal, .axisDelta = 300.0}, context);
    expect(result.effects.consume && near(result.effects.panDelta, 300.0), "idle mouse axis over the scene emits clamped pan and consumes");
    result = transitionInput({}, {.kind = EInputKind::MouseAxis, .globalLogicalPoint = {monitor.position.x - 1.0, monitor.position.y}, .axisDelta = 10.0}, context);
    expect(!result.effects.consume && near(result.effects.panDelta, 0.0), "outside mouse axis passes through");
    state = transitionInput({}, {.kind = EInputKind::MouseButton, .globalLogicalPoint = targetGlobal, .button = 0x111, .pressed = true}, context).state;
    result = transitionInput(state, {.kind = EInputKind::MouseAxis, .globalLogicalPoint = targetGlobal, .axisDelta = 50.0}, context);
    expect(result.effects.consume && near(result.effects.panDelta, 0.0), "axis during owned press is consumed without simultaneous pan");
}

void checkScrollingTouchAndResetState() {
    using namespace Hyprexpo;
    using namespace Hyprexpo::Scrolling;

    const auto scene = scrollingScene();
    const SMonitorGeometry monitor{.position = {}, .logicalSize = {1000.0, 500.0}, .pixelSize = {1000.0, 500.0}, .scale = 1.0};
    SInputContext context{.scene = &scene, .monitor = monitor, .pan = 0.0, .viewportHeight = 500.0, .dragThreshold = 12.0};
    const auto& target = scene.targets.front();
    const SPoint targetPoint{target.box.x + target.box.w / 2.0, target.box.y + target.box.h / 2.0};
    const SPoint background{900.0, 320.0};

    auto result = transitionInput({}, {.kind = EInputKind::TouchDown, .globalLogicalPoint = background, .touchId = 7}, context);
    expect(result.effects.consume && result.state.mode == EInputMode::CanvasPan && result.state.owningTouchId == 7, "touch background down owns canvas pan");
    auto state = result.state;
    result = transitionInput(state, {.kind = EInputKind::TouchMotion, .globalLogicalPoint = {900.0, 200.0}, .touchId = 7}, context);
    expect(result.effects.consume && near(result.effects.panDelta, 120.0) && !result.effects.beginDrag, "matching touch background motion pans without dragging");
    state = result.state;
    result = transitionInput(state, {.kind = EInputKind::TouchUp, .globalLogicalPoint = {}, .touchId = 7}, context);
    expect(result.effects.consume && result.effects.resetOwnership && !result.effects.selection, "touch canvas up ends without selection");

    result = transitionInput({}, {.kind = EInputKind::TouchDown, .globalLogicalPoint = targetPoint, .touchId = 9}, context);
    state = result.state;
    result = transitionInput(state, {.kind = EInputKind::TouchMotion, .globalLogicalPoint = {targetPoint.x + 4.0, targetPoint.y}, .touchId = 9}, context);
    state = result.state;
    result = transitionInput(state, {.kind = EInputKind::TouchUp, .globalLogicalPoint = {}, .touchId = 9}, context);
    expect(result.effects.consume && result.effects.selection && result.effects.selection->targetToken == target.token, "touch target tap selects exact pressed window");

    state = transitionInput({}, {.kind = EInputKind::TouchDown, .globalLogicalPoint = targetPoint, .touchId = 11}, context).state;
    result = transitionInput(state, {.kind = EInputKind::TouchCancel, .globalLogicalPoint = {}, .touchId = 12}, context);
    expect(!result.effects.consume && result.state.mode == EInputMode::TouchPressPending, "mismatched touch cancel passes through without releasing ownership");
    result = transitionInput(state, {.kind = EInputKind::TouchCancel, .globalLogicalPoint = {}, .touchId = 11}, context);
    expect(result.effects.consume && result.effects.cancelDrag && result.effects.resetOwnership && result.state.mode == EInputMode::Idle,
           "matching touch cancel clears pending ownership deterministically");
    state = result.state;
    result = transitionInput(state, {.kind = EInputKind::MouseButton, .globalLogicalPoint = targetPoint, .button = 0x111, .pressed = true}, context);
    expect(result.effects.consume && result.state.mode == EInputMode::MousePressPending, "mouse immediately reacquires after touch cancellation");

    state = transitionInput({}, {.kind = EInputKind::TouchDown, .globalLogicalPoint = targetPoint, .touchId = 13}, context).state;
    state = transitionInput(state, {.kind = EInputKind::TouchMotion, .globalLogicalPoint = {targetPoint.x + 20.0, targetPoint.y}, .touchId = 13}, context).state;
    result = transitionInput(state, {.kind = EInputKind::TouchCancel, .globalLogicalPoint = {}, .touchId = 13}, context);
    expect(result.effects.consume && result.effects.cancelDrag && result.state.mode == EInputMode::Idle, "touch cancel tears down an active drag");

    state = transitionInput({}, {.kind = EInputKind::MouseMove, .globalLogicalPoint = targetPoint}, context).state;
    auto reset = resetInput(state, EResetReason::Refresh);
    expect(reset.effects.resetOwnership && reset.effects.clearHover && reset.state.mode == EInputMode::Idle && reset.state.hover.kind == EHitKind::Outside,
           "refresh reset clears hover and ownership");
    auto repeated = resetInput(reset.state, EResetReason::Teardown);
    expect(repeated.effects.resetOwnership && repeated.state.mode == EInputMode::Idle, "repeated teardown reset remains idempotent");

    auto staleScene = scene;
    staleScene.targets.erase(staleScene.targets.begin());
    context.scene = &staleScene;
    state = transitionInput({}, {.kind = EInputKind::TouchDown, .globalLogicalPoint = targetPoint, .touchId = 15}, SInputContext{.scene = &scene, .monitor = monitor, .pan = 0.0, .viewportHeight = 500.0, .dragThreshold = 12.0}).state;
    result = transitionInput(state, {.kind = EInputKind::TouchMotion, .globalLogicalPoint = targetPoint, .touchId = 15}, context);
    expect(result.effects.consume && result.effects.cancelDrag && result.effects.resetOwnership && result.state.mode == EInputMode::Idle,
           "stale pressed target resets owned input instead of dereferencing it");
}

void checkScrollingCaptureBudget() {
    using namespace Hyprexpo::Scrolling;

    const std::vector<SCaptureRequest> requests{{.token = 1, .width = 3840, .height = 2160}, {.token = 2, .width = 1920, .height = 1080}};
    const auto defaults = planCaptureBudget(1920, 1080, 4, requests);
    expect(defaults.valid && defaults.multiplier == 4, "capture budget keeps the default multiplier");
    expect(defaults.budgetPixels == 4ULL * 1920ULL * 1080ULL, "capture budget is multiplier times monitor pixels");
    expect(defaults.allocations[0].width == 1920 && defaults.allocations[0].height == 1080, "each target is capped independently to monitor pixels");
    expect(near(defaults.scale, 1.0), "capture budget keeps full scale when capped requests fit");

    const auto constrained = planCaptureBudget(100, 100, 1, {{.token = 1, .width = 100, .height = 100}, {.token = 2, .width = 100, .height = 100}});
    expect(near(constrained.scale, std::sqrt(0.5)), "capture budget applies one shared square-root scale");
    expect(constrained.allocations[0].width == constrained.allocations[1].width, "shared scale produces equal dimensions for equal requests");

    expect(planCaptureBudget(100, 100, -9, {}).multiplier == 1, "capture multiplier clamps to one");
    expect(planCaptureBudget(100, 100, 99, {}).multiplier == 16, "capture multiplier clamps to sixteen");
    const auto tiny = planCaptureBudget(100, 100, 1, {{.token = 9, .width = 15, .height = 100}});
    expect(!tiny.allocations[0].capture && tiny.allocations[0].width == 0 && tiny.allocations[0].height == 0,
           "scaled dimensions below sixteen use a non-textured fallback");
    expect(!planCaptureBudget(0, 100, 4, requests).valid, "capture budget rejects invalid monitor dimensions");
}

Hyprexpo::Scrolling::SMutationState mutationFixture() {
    using namespace Hyprexpo::Scrolling;
    return {.workspaces = {
                {.workspaceID = 1,
                 .modelIdentity = 101,
                 .kind = EMutationWorkspaceKind::Scrolling,
                 .direction = "right",
                 .offset = 37.0,
                 .focusedTargetIdentity = 12,
                 .focusedWindowIdentity = 112,
                 .columns = {{.identity = 201, .width = 0.45, .targets = {{.identity = 11, .windowIdentity = 111, .size = 0.35}, {.identity = 12, .windowIdentity = 112, .size = 0.65}}},
                             {.identity = 202, .width = 0.70, .targets = {{.identity = 13, .windowIdentity = 113, .size = 1.0}}}},
                 .members = {11, 12, 13}},
                {.workspaceID = 2,
                 .modelIdentity = 102,
                 .kind = EMutationWorkspaceKind::Scrolling,
                 .direction = "right",
                 .offset = 9.0,
                 .focusedTargetIdentity = 21,
                 .focusedWindowIdentity = 121,
                 .columns = {{.identity = 203, .width = 0.55, .targets = {{.identity = 21, .windowIdentity = 121, .size = 1.0}}}},
                 .members = {21}},
                {.workspaceID = 3,
                 .modelIdentity = 0,
                 .kind = EMutationWorkspaceKind::Mixed,
                 .direction = {},
                 .offset = 0.0,
                 .columns = {},
                 .members = {31}},
            }};
}

void expectMutationCommit(const Hyprexpo::Scrolling::SMutationRequest& request, size_t workspaceIndex, size_t columnIndex, size_t rowIndex, const std::string& label) {
    using namespace Hyprexpo::Scrolling;
    const auto simulation = simulateMutation(mutationFixture(), request);
    expect(simulation.result.outcome == EMutationOutcome::Committed, label + " commits");
    expect(simulation.result.violatedInvariantIDs.empty(), label + " satisfies exact postconditions");
    expect(simulation.state.workspaces.at(workspaceIndex).columns.at(columnIndex).targets.at(rowIndex).identity == request.targetIdentity,
           label + " lands at the exact native position");
    expect(simulation.state.workspaces.front().direction == "right" && near(simulation.state.workspaces.front().offset, 37.0),
           label + " preserves controller direction and offset");
}

void checkScrollingMutationTransactions() {
    using namespace Hyprexpo::Scrolling;

    const auto emptyPreState = simulateMutation({}, {.targetIdentity = 11, .sourceWorkspaceID = 1, .destinationWorkspaceID = 1, .kind = EDropKind::ExistingColumn,
                                                     .placement = EColumnPlacement::Existing});
    expect(emptyPreState.result.outcome == EMutationOutcome::Rejected, "missing pre-state rejects without indexing an absent source workspace");

    const auto debugRequest = parseMutationDebugRequest("native.case-1 1 402653184 existing-column 1 2 1");
    expect(debugRequest.valid && debugRequest.requestID == "native.case-1" && debugRequest.targetStableID == 402653184 && debugRequest.kind == EDropKind::ExistingColumn,
           "native mutation debug request accepts the shared dotted ID and exact destination");
    const auto debugFault = parseMutationDebugRequest("rollback.case 1 402653184 new-before 1 0 0 apply:add-target:after");
    expect(debugFault.valid && debugFault.fault && debugFault.fault->phase == EMutationPhase::Apply && debugFault.fault->step == EMutationStep::AddTarget &&
               debugFault.fault->when == EFaultWhen::After,
           "native mutation debug request admits one bounded post-add rollback fault");
    expect(!parseMutationDebugRequest("bad/id 1 1 same-column 1 0 0").valid, "native mutation debug request shares the safe request ID grammar");
    expect(!parseMutationDebugRequest("case 1 1 unknown 1 0 0").valid, "native mutation debug request rejects unknown destination kinds");
    expect(!parseMutationDebugRequest("case 1 1 same-column 1 0 0 apply:remove-target:before").valid, "native mutation debug request rejects unapproved fault surfaces");

    expectMutationCommit({.targetIdentity = 11, .sourceWorkspaceID = 1, .destinationWorkspaceID = 1, .kind = EDropKind::ExistingColumn,
                          .placement = EColumnPlacement::Existing, .destinationColumnIndex = 0, .destinationRowIndex = 1},
                         0, 0, 1, "same-column reorder");
    expectMutationCommit({.targetIdentity = 13, .sourceWorkspaceID = 1, .destinationWorkspaceID = 1, .kind = EDropKind::ExistingColumn,
                          .placement = EColumnPlacement::Existing, .destinationColumnIndex = 0, .destinationRowIndex = 1},
                         0, 0, 1, "existing-column insertion");
    expectMutationCommit({.targetIdentity = 13, .sourceWorkspaceID = 1, .destinationWorkspaceID = 1, .kind = EDropKind::NewColumnBefore,
                          .placement = EColumnPlacement::Before, .destinationColumnIndex = 0, .destinationRowIndex = 0},
                         0, 0, 0, "new-column before with disappearing source");
    expectMutationCommit({.targetIdentity = 13, .sourceWorkspaceID = 1, .destinationWorkspaceID = 1, .kind = EDropKind::NewColumnAfter,
                          .placement = EColumnPlacement::After, .destinationColumnIndex = 1, .destinationRowIndex = 0},
                         0, 1, 0, "new-column after with disappearing source");
    expectMutationCommit({.targetIdentity = 11, .sourceWorkspaceID = 1, .destinationWorkspaceID = 2, .kind = EDropKind::CrossWorkspace,
                          .placement = EColumnPlacement::Existing, .destinationColumnIndex = 0, .destinationRowIndex = 1},
                         1, 0, 1, "cross-workspace insertion");

    auto mixed = simulateMutation(mutationFixture(), {.targetIdentity = 11, .sourceWorkspaceID = 1, .destinationWorkspaceID = 3, .kind = EDropKind::MixedFallback});
    expect(mixed.result.outcome == EMutationOutcome::Committed && mixed.result.violatedInvariantIDs.empty(), "mixed fallback commits controller ownership only");
    expect(mixed.state.workspaces[2].members == std::vector<uint64_t>({31, 11}) && mixed.state.workspaces[2].columns.empty(),
           "mixed fallback never invents native scrolling rows");

    auto terminalState = mutationFixture();
    auto terminal = simulateMutation(terminalState, {.targetIdentity = 11, .sourceWorkspaceID = 1, .destinationWorkspaceID = 4,
                                                     .kind = EDropKind::TerminalWorkspace, .createDestination = true});
    expect(terminal.result.outcome == EMutationOutcome::Committed && terminal.state.workspaces.back().workspaceID == 4,
           "terminal transaction creates the release-time next-empty workspace");
    expect(terminal.state.workspaces.back().columns.size() == 1 && terminal.state.workspaces.back().columns.front().targets.front().identity == 11,
           "terminal transaction moves exactly once into one native column");

    const SMutationRequest faultRequest{.targetIdentity = 11, .sourceWorkspaceID = 1, .destinationWorkspaceID = 2, .kind = EDropKind::CrossWorkspace,
                                         .placement = EColumnPlacement::Existing, .destinationColumnIndex = 0, .destinationRowIndex = 1};
    const auto successful = simulateMutation(mutationFixture(), faultRequest);
    expect(successful.result.outcome == EMutationOutcome::Committed && !successful.boundaries.empty(), "fault fixture has a complete successful operation trace");
    expect(successful.controllerMoveCount == 1 && successful.reverseMoveCount == 0, "cross-workspace commit invokes the controller exactly once");
    const auto reversed = simulateMutation(mutationFixture(), faultRequest,
                                           SFaultInjection{.phase = EMutationPhase::Apply, .step = EMutationStep::ReResolve, .when = EFaultWhen::After});
    expect(reversed.result.outcome == EMutationOutcome::RolledBack && reversed.controllerMoveCount == 1 && reversed.reverseMoveCount == 1,
           "post-controller failure invokes exactly one reverse move before exact rollback");
    const std::vector<std::pair<std::string, SMutationRequest>> matrix{
        {"same-column", {.targetIdentity = 11, .sourceWorkspaceID = 1, .destinationWorkspaceID = 1, .kind = EDropKind::ExistingColumn,
                          .placement = EColumnPlacement::Existing, .destinationColumnIndex = 0, .destinationRowIndex = 1}},
        {"existing-column", {.targetIdentity = 13, .sourceWorkspaceID = 1, .destinationWorkspaceID = 1, .kind = EDropKind::ExistingColumn,
                              .placement = EColumnPlacement::Existing, .destinationColumnIndex = 0, .destinationRowIndex = 1}},
        {"new-before", {.targetIdentity = 13, .sourceWorkspaceID = 1, .destinationWorkspaceID = 1, .kind = EDropKind::NewColumnBefore,
                         .placement = EColumnPlacement::Before, .destinationColumnIndex = 0}},
        {"new-after", {.targetIdentity = 13, .sourceWorkspaceID = 1, .destinationWorkspaceID = 1, .kind = EDropKind::NewColumnAfter,
                        .placement = EColumnPlacement::After, .destinationColumnIndex = 1}},
        {"cross", faultRequest},
        {"mixed", {.targetIdentity = 11, .sourceWorkspaceID = 1, .destinationWorkspaceID = 3, .kind = EDropKind::MixedFallback}},
        {"terminal", {.targetIdentity = 11, .sourceWorkspaceID = 1, .destinationWorkspaceID = 4, .kind = EDropKind::TerminalWorkspace, .createDestination = true}},
    };
    for (const auto& [label, request] : matrix) {
        const auto committed = simulateMutation(mutationFixture(), request);
        expect(committed.result.outcome == EMutationOutcome::Committed, label + " baseline commits before fault enumeration");
        const auto hasBoundary = [](const SMutationSimulation& simulation, EMutationPhase phase, EMutationStep step, EFaultWhen when) {
            return std::ranges::any_of(simulation.boundaries, [&](const auto& boundary) {
                return boundary.phase == phase && boundary.step == step && boundary.when == when;
            });
        };
        std::vector<EMutationStep> applySteps{EMutationStep::SnapshotPreState, EMutationStep::RestoreWidths, EMutationStep::RestoreSizes,
                                              EMutationStep::Recalculate, EMutationStep::SnapshotPostState, EMutationStep::VerifyPostconditions};
        if (request.sourceWorkspaceID == request.destinationWorkspaceID) {
            applySteps.push_back(EMutationStep::RemoveTarget);
            applySteps.push_back(EMutationStep::AddTarget);
        } else {
            applySteps.push_back(EMutationStep::ControllerMove);
            applySteps.push_back(EMutationStep::ReResolve);
            if (request.kind != EDropKind::MixedFallback)
                applySteps.push_back(EMutationStep::AddTarget);
        }
        for (const auto step : applySteps)
            for (const auto when : {EFaultWhen::Before, EFaultWhen::After})
                expect(hasBoundary(committed, EMutationPhase::Apply, step, when), label + " exposes every required apply boundary before and after");
        for (const auto& boundary : committed.boundaries) {
            if (boundary.phase != EMutationPhase::Apply)
                continue;
            const auto failed = simulateMutation(mutationFixture(), request, boundary);
            const bool preSnapshotBefore = boundary.step == EMutationStep::SnapshotPreState && boundary.when == EFaultWhen::Before;
            expect(failed.result.outcome == (preSnapshotBefore ? EMutationOutcome::Rejected : EMutationOutcome::RolledBack),
                   label + " apply boundary returns the phase-correct outcome");
            expect(failed.state == mutationFixture(), label + " apply boundary never mutates or restores exact pre-state");
        }

        const auto rollbackTrace = simulateMutation(mutationFixture(), request,
                                                     SFaultInjection{.phase = EMutationPhase::Rollback, .step = EMutationStep::VerifyPostconditions,
                                                                     .when = EFaultWhen::After});
        std::vector<EMutationStep> rollbackSteps{EMutationStep::RestorePreState, EMutationStep::RestoreWidths, EMutationStep::RestoreSizes,
                                                 EMutationStep::Recalculate, EMutationStep::SnapshotPostState, EMutationStep::VerifyPostconditions};
        if (request.sourceWorkspaceID != request.destinationWorkspaceID) {
            rollbackSteps.push_back(EMutationStep::ReverseControllerMove);
            rollbackSteps.push_back(EMutationStep::ReResolve);
        }
        for (const auto step : rollbackSteps)
            for (const auto when : {EFaultWhen::Before, EFaultWhen::After})
                expect(hasBoundary(rollbackTrace, EMutationPhase::Rollback, step, when), label + " exposes every required rollback boundary before and after");
        for (const auto& boundary : rollbackTrace.boundaries) {
            if (boundary.phase != EMutationPhase::Rollback)
                continue;
            const auto fatal = simulateMutation(mutationFixture(), request, boundary);
            expect(fatal.result.outcome == EMutationOutcome::RollbackFailed && !fatal.result.violatedInvariantIDs.empty(),
                   label + " rollback boundary is fatal-safe with invariant IDs");
        }
    }

    const auto sameColumn = simulateMutation(mutationFixture(), matrix[0].second);
    expect(near(sameColumn.state.workspaces[0].columns[0].targets[0].size, 0.65) && near(sameColumn.state.workspaces[0].columns[0].targets[1].size, 0.35),
           "same-column reorder preserves exact per-target proportions while changing order");
    const auto existingColumn = simulateMutation(mutationFixture(), matrix[1].second);
    const auto& inserted = existingColumn.state.workspaces[0].columns[0].targets;
    const double insertedSum = inserted[0].size + inserted[1].size + inserted[2].size;
    expect(near(insertedSum, 1.0) && near(inserted[1].size, 1.0 / 3.0), "existing-column insertion allocates one bounded native row share and sums to one");
    expect(near(inserted[0].size / inserted[2].size, 0.35 / 0.65), "existing destination rows preserve their relative size ratio");
    expect(near(successful.state.workspaces[1].columns[0].width, 0.55), "cross insertion preserves the existing destination column width");

    auto duplicate = successful.state;
    duplicate.workspaces[1].members.push_back(11);
    const auto duplicateViolations = verifyPostconditions(successful.result.before, duplicate, successful.result.plan, false);
    expect(std::ranges::find(duplicateViolations, "membership.exact-once") != duplicateViolations.end(), "exact-once comparator rejects duplicate targets");
    auto lost = successful.state;
    std::erase(lost.workspaces[1].members, 11);
    const auto lostViolations = verifyPostconditions(successful.result.before, lost, successful.result.plan, false);
    expect(std::ranges::find(lostViolations, "membership.exact-once") != lostViolations.end(), "exact-once comparator rejects lost targets");
    auto foreign = successful.state;
    foreign.workspaces[1].members.push_back(999);
    const auto foreignViolations = verifyPostconditions(successful.result.before, foreign, successful.result.plan, false);
    expect(std::ranges::find(foreignViolations, "membership.exact-once") != foreignViolations.end(), "exact-once comparator rejects foreign targets");
    auto misplaced = successful.state;
    std::swap(misplaced.workspaces[1].columns[0].targets[0], misplaced.workspaces[1].columns[0].targets[1]);
    const auto misplacedViolations = verifyPostconditions(successful.result.before, misplaced, successful.result.plan, false);
    expect(std::ranges::find(misplacedViolations, "moved.expected-location") != misplacedViolations.end(), "postconditions reject a misplaced moved target");
    auto widened = successful.state;
    widened.workspaces[1].columns[0].width = 0.25;
    const auto widthViolations = verifyPostconditions(successful.result.before, widened, successful.result.plan, false);
    expect(std::ranges::find(widthViolations, "unaffected.width") != widthViolations.end(), "postconditions reject changed unaffected widths");
    auto resized = successful.state;
    resized.workspaces[1].columns[0].targets[0].size = 0.25;
    const auto sizeViolations = verifyPostconditions(successful.result.before, resized, successful.result.plan, false);
    expect(std::ranges::find(sizeViolations, "size.expected") != sizeViolations.end(), "postconditions reject a changed normalized destination row size");
    auto invalidAggregate = successful.state;
    invalidAggregate.workspaces[1].columns[0].targets[0].size += 0.1;
    const auto aggregateViolations = verifyPostconditions(successful.result.before, invalidAggregate, successful.result.plan, false);
    expect(std::ranges::find(aggregateViolations, "size.aggregate") != aggregateViolations.end(), "postconditions reject non-unit destination row totals");
    auto staleSource = successful.state;
    staleSource.workspaces[0].columns[0].targets.push_back({.identity = 11, .windowIdentity = 111, .size = 0.5});
    const auto staleSourceViolations = verifyPostconditions(successful.result.before, staleSource, successful.result.plan, false);
    expect(std::ranges::find(staleSourceViolations, "topology.exact-once") != staleSourceViolations.end(),
           "global exact-once rejects a stale source topology copy plus destination copy");

    const auto terminalRollback = simulateMutation(mutationFixture(), matrix.back().second,
                                                    SFaultInjection{.phase = EMutationPhase::Apply, .step = EMutationStep::SnapshotPostState,
                                                                    .when = EFaultWhen::Before});
    expect(terminalRollback.result.outcome == EMutationOutcome::RolledBack && terminalRollback.state == mutationFixture(),
           "terminal rollback proves and removes the created empty destination from exact readback");

    expect(nextUnusedOrdinaryWorkspaceID({1, 3, -99, 4}) == 2, "terminal allocation chooses the first globally unused positive ordinary workspace ID");
    expect(nextUnusedOrdinaryWorkspaceID({1, 2, 3}) == 4, "terminal allocation advances past a dense positive workspace prefix");
}

}

int main() {
    using namespace Hyprexpo;

    expect(trimString("  DP-1 first 1 \t") == "DP-1 first 1", "trimString removes surrounding whitespace");
    expect(splitCommaList("a, b,,c").size() == 4, "splitCommaList preserves empty entries");

    expect(clampGridColumns(-1) == 1, "columns clamp lower bound");
    expect(clampGridColumns(3) == 3, "columns keep valid value");
    expect(clampGridColumns(99) == 7, "columns clamp upper bound");

    const auto wideFixed = computeFixedGridShape(5, 2);
    expect(wideFixed.cols == 5 && wideFixed.rows == 2 && wideFixed.cols * wideFixed.rows == 10,
           "explicit five-column two-row fixed grid contains ten slots");
    const auto tallFixed = computeFixedGridShape(2, 5);
    expect(tallFixed.cols == 2 && tallFixed.rows == 5, "explicit rows can exceed columns");
    const auto defaultFixed = computeFixedGridShape(3, 0);
    expect(defaultFixed.cols == 3 && defaultFixed.rows == 3, "unset rows preserve the legacy square grid");
    const auto negativeRows = computeFixedGridShape(4, std::numeric_limits<int64_t>::min());
    expect(negativeRows.cols == 4 && negativeRows.rows == 4, "nonpositive rows use the configured column count");
    const auto oversizedFixed = computeFixedGridShape(std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max());
    expect(oversizedFixed.cols == 7 && oversizedFixed.rows == 7, "64-bit grid settings clamp before narrowing or allocation");
    const auto minimumFixed = computeFixedGridShape(std::numeric_limits<int64_t>::min(), 1);
    expect(minimumFixed.cols == 1 && minimumFixed.rows == 1, "minimum column setting remains a valid bounded grid");

    expect(gridColumnsToIncludeWorkspace(3, 1, 9, 7) == 3, "first-anchor grid keeps columns when active workspace fits");
    expect(gridColumnsToIncludeWorkspace(3, 1, 10, 7) == 4, "first-anchor grid grows to include an active workspace just past the grid");
    expect(gridColumnsToIncludeWorkspace(3, 1, 16, 7) == 4, "first-anchor grid grows to exactly fill the last tile");
    expect(gridColumnsToIncludeWorkspace(3, 1, 17, 7) == 5, "first-anchor grid grows another column past a full grid");
    expect(gridColumnsToIncludeWorkspace(3, 5, 12, 7) == 3, "first-anchor grid accounts for a non-1 anchor when sizing");
    expect(gridColumnsToIncludeWorkspace(3, 5, 4, 7) == 3, "first-anchor grid never shrinks for an active workspace before the anchor");
    expect(gridColumnsToIncludeWorkspace(3, 1, 1000, 7) == 7, "first-anchor grid is capped at the max columns as a best effort");
    expect(gridColumnsToIncludeWorkspace(5, 1, 4, 7) == 5, "first-anchor grid never shrinks below the configured columns");

    expect(gridColumnsToIncludeWorkspace(5, 1, 10, 7, 2) == 5, "five-by-two grid keeps its shape when the active workspace fits");
    expect(gridColumnsToIncludeWorkspace(3, 1, 7, 7, 2) == 4, "rectangular first-anchor growth uses the explicit row count");
    expect(gridColumnsToIncludeWorkspace(5, 1, 11, 7, 2) == 6, "rectangular growth adds only the needed column");
    expect(gridColumnsToIncludeWorkspace(2, 6, 15, 7, 5) == 2, "tall fixed grid accounts for a non-one first anchor");
    expect(gridColumnsToIncludeWorkspace(2, 6, 16, 7, 5) == 3, "tall grid grows columns without changing rows");
    expect(gridColumnsToIncludeWorkspace(3, 1, 1000, 7, 1) == 7, "single-row growth remains bounded");
    expect(gridColumnsToIncludeWorkspace(5, 1, 2, 7, 2) == 5, "rectangular growth never shrinks the configured width");

    expect(centeredWorkspaceBacktrack(9, 1, 1, 9) == 0, "center-current keeps the low workspace at the first tile");
    expect(centeredWorkspaceBacktrack(9, 5, 1, 9) == 4, "center-current places the middle workspace at the center tile");
    expect(centeredWorkspaceBacktrack(9, 9, 1, 9) == 8, "center-current slides the high workspace back inside the real bounds");
    expect(centeredWorkspaceBacktrack(9, 11, 11, 19) == 0, "center-current supports a shifted range at its low workspace");
    expect(centeredWorkspaceBacktrack(9, 15, 11, 19) == 4, "center-current supports a shifted range at its middle workspace");
    expect(centeredWorkspaceBacktrack(9, 19, 11, 19) == 8, "center-current supports a shifted range at its high workspace");
    expect(centeredWorkspaceBacktrack(9, 7, 6, 9) == 1, "center-current respects a short monitor-local range");
    expect(centeredWorkspaceBacktrack(9, 5, std::nullopt, std::nullopt) == 3, "skip-empty 3x3 traversal preserves its three predecessor queries");
    expect(centeredWorkspaceBacktrack(0, 5, 1, 9) == 0, "center-current handles an empty tile set");
    expect(centeredWorkspaceBacktrack(9, 5, 9, 1) == 4, "center-current ignores reversed bounds safely");
    expect(centeredWorkspaceBacktrack(9, std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()) == 8,
           "center-current handles the full signed workspace range without overflow");

    expect(HyprexpoConfig::SHOW_PINNED_WINDOWS_DEFAULT == 0, "pinned windows are hidden from previews by default");
    expect(!shouldAbortOverviewCloseForWorkspaceMove(true, true), "pinned moves on the overview monitor preserve the close animation");
    expect(shouldAbortOverviewCloseForWorkspaceMove(false, true), "non-pinned moves on the overview monitor abort the close animation");
    expect(!shouldAbortOverviewCloseForWorkspaceMove(false, false), "non-pinned moves outside the overview monitor are ignored");
    expect(!shouldAbortOverviewCloseForWorkspaceMove(true, false), "pinned moves outside the overview monitor are ignored");
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
    checkFixedGridGeometryForMonitor(makeSize(1600, 900));
    checkFixedGridGeometryForMonitor(makeSize(900, 1600));
    checkFixedGridGeometryForMonitor(makeSize(1280, 720.5));
    checkScrollingTapeDirections();
    checkScrollingSceneAndInputMath();
    checkScrollingDropIntents();
    checkScrollingCaptureBudget();
    checkScrollingInputCoordinates();
    checkScrollingRequestIds();
    checkScrollingOverviewTransition();
    checkScrollingMouseInputState();
    checkScrollingTouchAndResetState();
    checkScrollingMutationTransactions();

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
