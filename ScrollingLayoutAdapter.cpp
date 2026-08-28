#include "ScrollingLayoutAdapter.hpp"

#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp>
#include <hyprland/src/layout/supplementary/WorkspaceAlgoMatcher.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/output/Monitor.hpp>

#include <cmath>
#include <exception>

namespace Hyprexpo::Scrolling {

namespace {

uintptr_t fingerprintPointer(const void* pointer) {
    if (!pointer)
        return 0;
    static const int saltAnchor = 0;
    uint64_t value = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer) ^ reinterpret_cast<uintptr_t>(&saltAnchor));
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31;
    return static_cast<uintptr_t>(value);
}

template <typename T>
uintptr_t fingerprint(const SP<T>& value) {
    return fingerprintPointer(value.get());
}

bool validPositive(double value) {
    return std::isfinite(value) && value > 0.0;
}

bool validBox(const CBox& box) {
    return std::isfinite(box.x) && std::isfinite(box.y) && validPositive(box.w) && validPositive(box.h);
}

std::string directionName(Layout::Tiled::eScrollDirection direction) {
    switch (direction) {
        case Layout::Tiled::SCROLL_DIR_RIGHT: return "right";
        case Layout::Tiled::SCROLL_DIR_LEFT: return "left";
        case Layout::Tiled::SCROLL_DIR_DOWN: return "down";
        case Layout::Tiled::SCROLL_DIR_UP: return "up";
    }
    return "unknown";
}

SSnapshotResult failure(ESnapshotFailure code, std::string error) {
    return {.failure = code, .error = std::move(error), .snapshot = std::nullopt};
}

STargetSnapshot copyTarget(const SP<Layout::ITarget>& target, const CBox& layoutBox, size_t rowIndex, float proportion, bool visible) {
    const auto window = target->window();
    return {
        .rowIndex = rowIndex,
        .proportion = proportion,
        .targetFingerprint = fingerprint(target),
        .windowFingerprint = fingerprint(window),
        .windowStableID = window ? window->m_stableID : 0,
        .layoutBox = layoutBox,
        .group = target->type() == Layout::TARGET_TYPE_GROUP,
        .floating = target->floating(),
        .fullscreen = window && Fullscreen::controller() && Fullscreen::controller()->isFullscreen(window),
        .pinned = window && window->m_pinned,
        .visible = visible,
        .targetRef = target,
        .windowRef = window,
    };
}

SSnapshotResult snapshotWorkspaceImpl(const PHLWORKSPACE& workspace) {
    if (!workspace)
        return failure(ESnapshotFailure::NullWorkspace, "workspace is null");
    if (workspace->inert())
        return failure(ESnapshotFailure::InertWorkspace, "workspace is inert");

    const auto monitor = workspace->m_monitor.lock();
    if (!monitor)
        return failure(ESnapshotFailure::MissingMonitor, "workspace monitor expired");
    if (!workspace->m_space)
        return failure(ESnapshotFailure::MissingSpace, "workspace space is unavailable");

    const auto algorithmOwner = workspace->m_space->algorithm();
    if (!algorithmOwner)
        return failure(ESnapshotFailure::MissingAlgorithm, "workspace algorithm is unavailable");
    const auto& tiledOwner = algorithmOwner->tiledAlgo();
    if (!tiledOwner)
        return failure(ESnapshotFailure::MissingTiledAlgorithm, "workspace tiled algorithm is unavailable");

    auto* const tiled = tiledOwner.get();
    if (Layout::Supplementary::algoMatcher()->getNameForTiledAlgo(tiled) != "scrolling")
        return failure(ESnapshotFailure::WrongAlgorithmName, "workspace tiled algorithm is not scrolling");

    auto* const scrolling = dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>(tiled);
    if (!scrolling)
        return failure(ESnapshotFailure::CastFailure, "scrolling matcher succeeded but dynamic cast failed");

    std::vector<SP<Layout::ITarget>> layoutTargets;
    layoutTargets.reserve(workspace->m_space->targets().size());
    for (const auto& targetRef : workspace->m_space->targets()) {
        const auto target = targetRef.lock();
        if (!target)
            return failure(ESnapshotFailure::ExpiredTarget, "workspace target expired while resolving scrolling data");
        layoutTargets.push_back(target);
    }

    SP<Layout::Tiled::SScrollingData> data;
    for (const auto& target : layoutTargets) {
        if (target->floating())
            continue;
        const auto targetData = scrolling->dataFor(target);
        if (!targetData)
            continue;
        const auto column = targetData->column.lock();
        if (!column)
            return failure(ESnapshotFailure::ExpiredColumn, "scrolling target column expired");
        data = column->scrollingData.lock();
        if (!data)
            return failure(ESnapshotFailure::ExpiredData, "scrolling column data expired");
        break;
    }

    if (!data || !data->controller)
        return failure(ESnapshotFailure::MissingScrollingData, "no live scrolling data/controller is available");
    const auto& controller = *data->controller;
    if (data->columns.size() != controller.stripCount())
        return failure(ESnapshotFailure::ColumnCardinalityMismatch, "column/controller strip cardinality mismatch");

    SWorkspaceSnapshot snapshot{
        .workspaceID = workspace->m_id,
        .monitorID = monitor->m_id,
        .algorithmFingerprint = fingerprintPointer(scrolling),
        .dataFingerprint = fingerprint(data),
        .direction = directionName(controller.getDirection()),
        .offset = controller.getOffset(),
        .activeWorkspaceID = monitor->m_activeWorkspace ? monitor->m_activeWorkspace->m_id : 0,
        .focusedWindowFingerprint = fingerprint(Desktop::focusState() ? Desktop::focusState()->window() : PHLWINDOW{}),
        .columns = {},
        .layoutTargets = {},
    };
    if (!std::isfinite(snapshot.offset))
        return failure(ESnapshotFailure::InvalidGeometry, "scrolling offset is not finite");

    const auto usable = scrolling->usableArea();
    if (!validBox(usable))
        return failure(ESnapshotFailure::InvalidGeometry, "scrolling usable area is invalid");
    const bool horizontal = controller.getDirection() == Layout::Tiled::SCROLL_DIR_RIGHT || controller.getDirection() == Layout::Tiled::SCROLL_DIR_LEFT;
    const double viewportStart = horizontal ? usable.x : usable.y;
    const double viewportEnd = viewportStart + (horizontal ? usable.w : usable.h);

    snapshot.columns.reserve(data->columns.size());
    for (size_t columnIndex = 0; columnIndex < data->columns.size(); ++columnIndex) {
        const auto& column = data->columns[columnIndex];
        if (!column)
            return failure(ESnapshotFailure::ExpiredColumn, "scrolling column is null");
        const auto columnData = column->scrollingData.lock();
        if (!columnData || columnData != data)
            return failure(ESnapshotFailure::ExpiredData, "scrolling column owner expired or changed");

        const auto& strip = controller.getStrip(columnIndex);
        const auto stripColumn = strip.userData.lock();
        if (!stripColumn || stripColumn != column)
            return failure(ESnapshotFailure::ExpiredColumn, "controller strip column expired or changed");
        if (strip.targetSizes.size() != column->targetDatas.size())
            return failure(ESnapshotFailure::TargetCardinalityMismatch, "column/controller target cardinality mismatch");
        if (!validPositive(strip.size))
            return failure(ESnapshotFailure::InvalidGeometry, "scrolling column width is invalid");

        const double primaryStart = controller.calculateStripStart(columnIndex, usable);
        const double primarySize = controller.calculateStripSize(columnIndex, usable);
        if (!std::isfinite(primaryStart) || !validPositive(primarySize))
            return failure(ESnapshotFailure::InvalidGeometry, "scrolling column placement is invalid");
        const bool visible = primaryStart < viewportEnd && primaryStart + primarySize > viewportStart;

        SColumnSnapshot columnSnapshot{
            .index = columnIndex,
            .fingerprint = fingerprint(column),
            .width = strip.size,
            .primaryStart = primaryStart,
            .primarySize = primarySize,
            .visible = visible,
            .targets = {},
        };
        columnSnapshot.targets.reserve(column->targetDatas.size());
        for (size_t rowIndex = 0; rowIndex < column->targetDatas.size(); ++rowIndex) {
            const auto& targetData = column->targetDatas[rowIndex];
            if (!targetData)
                return failure(ESnapshotFailure::ExpiredTarget, "scrolling target data is null");
            const auto targetColumn = targetData->column.lock();
            if (!targetColumn || targetColumn != column)
                return failure(ESnapshotFailure::ExpiredColumn, "scrolling target column expired or changed");
            const auto target = targetData->target.lock();
            if (!target)
                return failure(ESnapshotFailure::ExpiredTarget, "scrolling target expired while copying topology");
            if (!validPositive(strip.targetSizes[rowIndex]) || !validBox(targetData->layoutBox))
                return failure(ESnapshotFailure::InvalidGeometry, "scrolling target size/layout box is invalid");
            columnSnapshot.targets.push_back(copyTarget(target, targetData->layoutBox, rowIndex, strip.targetSizes[rowIndex], visible));
        }
        snapshot.columns.push_back(std::move(columnSnapshot));
    }

    snapshot.layoutTargets.reserve(layoutTargets.size());
    for (const auto& target : layoutTargets) {
        const auto box = target->position();
        if (!validBox(box))
            return failure(ESnapshotFailure::InvalidGeometry, "layout target box is invalid");
        snapshot.layoutTargets.push_back(copyTarget(target, box, 0, 0.F, workspace->m_visible));
    }

    return {.failure = ESnapshotFailure::None, .error = {}, .snapshot = std::move(snapshot)};
}

} // namespace

std::string snapshotFailureName(ESnapshotFailure failureCode) {
    switch (failureCode) {
        case ESnapshotFailure::None: return "none";
        case ESnapshotFailure::NullWorkspace: return "null-workspace";
        case ESnapshotFailure::InertWorkspace: return "inert-workspace";
        case ESnapshotFailure::MissingMonitor: return "missing-monitor";
        case ESnapshotFailure::MissingSpace: return "missing-space";
        case ESnapshotFailure::MissingAlgorithm: return "missing-algorithm";
        case ESnapshotFailure::MissingTiledAlgorithm: return "missing-tiled-algorithm";
        case ESnapshotFailure::WrongAlgorithmName: return "wrong-algorithm-name";
        case ESnapshotFailure::CastFailure: return "cast-failure";
        case ESnapshotFailure::ExpiredTarget: return "expired-target";
        case ESnapshotFailure::ExpiredColumn: return "expired-column";
        case ESnapshotFailure::ExpiredData: return "expired-data";
        case ESnapshotFailure::MissingScrollingData: return "missing-scrolling-data";
        case ESnapshotFailure::ColumnCardinalityMismatch: return "column-cardinality-mismatch";
        case ESnapshotFailure::TargetCardinalityMismatch: return "target-cardinality-mismatch";
        case ESnapshotFailure::InvalidGeometry: return "invalid-geometry";
        case ESnapshotFailure::HostException: return "host-exception";
    }
    return "unknown";
}

SSnapshotResult snapshotWorkspace(const PHLWORKSPACE& workspace) {
    try {
        return snapshotWorkspaceImpl(workspace);
    } catch (const std::exception& error) {
        return failure(ESnapshotFailure::HostException, error.what());
    } catch (...) {
        return failure(ESnapshotFailure::HostException, "unknown host exception");
    }
}

bool workspaceUsesScrollingLayout(const PHLWORKSPACE& workspace) {
    try {
        if (!workspace || workspace->inert() || !workspace->m_space)
            return false;
        const auto algorithmOwner = workspace->m_space->algorithm();
        if (!algorithmOwner)
            return false;
        const auto& tiledOwner = algorithmOwner->tiledAlgo();
        if (!tiledOwner)
            return false;
        auto* const tiled = tiledOwner.get();
        if (Layout::Supplementary::algoMatcher()->getNameForTiledAlgo(tiled) != "scrolling")
            return false;
        return dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>(tiled) != nullptr;
    } catch (...) {
        return false;
    }
}

}
