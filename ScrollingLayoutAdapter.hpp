#pragma once

#include "ScrollingMutationTransaction.hpp"

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprland/src/layout/target/Target.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Hyprexpo::Scrolling {

enum class ESnapshotFailure {
    None,
    NullWorkspace,
    InertWorkspace,
    MissingMonitor,
    MissingSpace,
    MissingAlgorithm,
    MissingTiledAlgorithm,
    WrongAlgorithmName,
    CastFailure,
    ExpiredTarget,
    ExpiredColumn,
    ExpiredData,
    MissingScrollingData,
    ColumnCardinalityMismatch,
    TargetCardinalityMismatch,
    InvalidGeometry,
    HostException,
};

struct STargetSnapshot {
    size_t              rowIndex = 0;
    float               proportion = 0.F;
    uintptr_t           targetFingerprint = 0;
    uintptr_t           windowFingerprint = 0;
    uint64_t            windowStableID = 0;
    CBox                layoutBox;
    bool                group = false;
    bool                floating = false;
    bool                fullscreen = false;
    bool                pinned = false;
    bool                visible = false;
    WP<Layout::ITarget> targetRef;
    PHLWINDOWREF        windowRef;
};

struct SColumnSnapshot {
    size_t                       index = 0;
    uintptr_t                    fingerprint = 0;
    double                       width = 0.0;
    double                       primaryStart = 0.0;
    double                       primarySize = 0.0;
    bool                         visible = false;
    std::vector<STargetSnapshot> targets;
};

struct SWorkspaceSnapshot {
    int64_t                      workspaceID = 0;
    int64_t                      monitorID = 0;
    uintptr_t                    algorithmFingerprint = 0;
    uintptr_t                    dataFingerprint = 0;
    std::string                  direction;
    double                       offset = 0.0;
    int64_t                      activeWorkspaceID = 0;
    uintptr_t                    focusedWindowFingerprint = 0;
    std::vector<SColumnSnapshot> columns;
    std::vector<STargetSnapshot> layoutTargets;
};

struct SSnapshotResult {
    ESnapshotFailure                  failure = ESnapshotFailure::None;
    std::string                       error;
    std::optional<SWorkspaceSnapshot> snapshot;

    bool success() const {
        return failure == ESnapshotFailure::None && snapshot.has_value();
    }
};

std::string     snapshotFailureName(ESnapshotFailure failure);
SSnapshotResult snapshotWorkspace(const PHLWORKSPACE& workspace);
bool            workspaceUsesScrollingLayout(const PHLWORKSPACE& workspace);
SMutationResult moveScrollingTarget(const PHLWORKSPACE& sourceWorkspace, const PHLWORKSPACE& destinationWorkspace, const SMutationRequest& request);

}
