#include "../HyprexpoLogic.hpp"
#include "../HyprexpoConfig.hpp"
#include "../ScrollingOverviewLogic.hpp"
#include "../ScrollingInputState.hpp"
#include "../ScrollingMutationTransaction.hpp"

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
    expect(successful.result.outcome == EMutationOutcome::Committed && !successful.trace.empty(), "fault fixture has a complete successful operation trace");
    for (const auto step : successful.trace) {
        for (const auto when : {EFaultWhen::Before, EFaultWhen::After}) {
            const auto failed = simulateMutation(mutationFixture(), faultRequest, SFaultInjection{.phase = EMutationPhase::Apply, .step = step, .when = when});
            expect(failed.result.outcome == EMutationOutcome::RolledBack, "fault before/after each apply boundary reports rollback");
            expect(failed.state == mutationFixture(), "fault before/after each apply boundary restores byte-for-structure pre-state");
            expect(failed.result.violatedInvariantIDs.empty(), "fault rollback satisfies exact equality postconditions");
        }
    }

    const auto fatal = simulateMutation(mutationFixture(), faultRequest,
                                        SFaultInjection{.phase = EMutationPhase::Rollback, .step = EMutationStep::RestorePreState, .when = EFaultWhen::Before});
    expect(fatal.result.outcome == EMutationOutcome::RollbackFailed && !fatal.result.violatedInvariantIDs.empty(),
           "rollback boundary failure is explicit and carries violated invariant IDs");

    auto duplicate = successful.state;
    duplicate.workspaces[1].members.push_back(11);
    const auto duplicateViolations = verifyPostconditions(successful.result.before, duplicate, successful.result.plan, false);
    expect(std::ranges::find(duplicateViolations, "membership.exact-once") != duplicateViolations.end(), "exact-once comparator rejects duplicate targets");
    auto resized = successful.state;
    resized.workspaces[1].columns[0].targets[0].size = 0.25;
    const auto sizeViolations = verifyPostconditions(successful.result.before, resized, successful.result.plan, false);
    expect(std::ranges::find(sizeViolations, "unaffected.size") != sizeViolations.end(), "postconditions reject changed unaffected row sizes");
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
    checkScrollingTapeDirections();
    checkScrollingSceneAndInputMath();
    checkScrollingDropIntents();
    checkScrollingCaptureBudget();
    checkScrollingInputCoordinates();
    checkScrollingMouseInputState();
    checkScrollingTouchAndResetState();
    checkScrollingMutationTransactions();

    if (failures != 0)
        return 1;

    std::cout << "HyprexpoLogicTests passed\n";
    return 0;
}
