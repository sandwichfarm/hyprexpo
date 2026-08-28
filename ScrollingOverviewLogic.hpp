#pragma once

#include "HyprexpoLogic.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Hyprexpo::Scrolling {

enum class EDirection {
    Right,
    Left,
    Down,
    Up,
};

enum class EWorkspaceKind {
    Scrolling,
    Empty,
    Mixed,
    Terminal,
};

enum class EHitKind {
    Outside,
    Target,
    EmptyWorkspace,
    MixedWorkspace,
    TerminalWorkspace,
};

enum class EFocusDirection {
    Left,
    Right,
    Up,
    Down,
};

enum class EColumnPlacement {
    None,
    Existing,
    Before,
    After,
};

enum class EDropKind {
    Invalid,
    NoOp,
    ExistingColumn,
    NewColumnBefore,
    NewColumnAfter,
    CrossWorkspace,
    MixedFallback,
    TerminalWorkspace,
};

struct STargetSpec {
    uint64_t token      = 0;
    double   proportion = 0.0;
};

struct SColumnSpec {
    uint64_t                 token  = 0;
    double                   extent = 0.0;
    std::vector<STargetSpec> targets;
};

struct STapeSpec {
    EDirection              direction = EDirection::Right;
    std::vector<SColumnSpec> columns;
};

struct SPlacedTarget {
    uint64_t token             = 0;
    uint64_t columnToken       = 0;
    size_t   nativeColumnIndex = 0;
    size_t   nativeRowIndex    = 0;
    SRect    box;
};

struct STapeLayout {
    bool                       valid = false;
    std::string                error;
    SRect                      bounds;
    std::vector<SPlacedTarget> targets;
};

struct SWorkspaceSpec {
    int64_t        workspaceID = 0;
    EWorkspaceKind kind        = EWorkspaceKind::Scrolling;
    STapeSpec      tape;
};

struct SSceneConfig {
    double  viewportWidth      = 0.0;
    double  viewportHeight     = 0.0;
    double  rowHeight          = 0.0;
    double  rowGap             = 0.0;
    double  columnGap          = 0.0;
    int64_t terminalWorkspaceID = 0;
};

struct SPlacedWorkspace {
    int64_t        workspaceID = 0;
    EWorkspaceKind kind        = EWorkspaceKind::Scrolling;
    EDirection     direction   = EDirection::Right;
    SRect          box;
};

struct SSceneTarget : SPlacedTarget {
    int64_t workspaceID = 0;
};

struct SScene {
    bool                          valid = false;
    std::string                   error;
    double                        contentHeight = 0.0;
    std::vector<SPlacedWorkspace> workspaces;
    std::vector<SSceneTarget>     targets;
};

struct SHitResult {
    EHitKind kind        = EHitKind::Outside;
    int64_t  workspaceID = 0;
    uint64_t targetToken = 0;
    size_t   columnIndex = 0;
    size_t   rowIndex    = 0;
};

struct SFocusRef {
    EHitKind kind        = EHitKind::Outside;
    int64_t  workspaceID = 0;
    uint64_t targetToken = 0;
};

struct SDropSource {
    int64_t workspaceID             = 0;
    size_t  columnIndex             = 0;
    size_t  rowIndex                = 0;
    bool    sourceColumnWillDisappear = false;
};

struct SDropIntent {
    EDropKind       kind                  = EDropKind::Invalid;
    EColumnPlacement placement            = EColumnPlacement::None;
    int64_t          workspaceID           = 0;
    size_t           columnIndex           = 0;
    size_t           adjustedColumnIndex   = 0;
    size_t           rowIndex              = 0;
};

struct SCaptureRequest {
    uint64_t token  = 0;
    uint32_t width  = 0;
    uint32_t height = 0;
};

struct SCaptureAllocation {
    uint64_t token   = 0;
    uint32_t width   = 0;
    uint32_t height  = 0;
    bool     capture = false;
};

struct SCapturePlan {
    bool                            valid = false;
    int                             multiplier = 4;
    uint64_t                        budgetPixels = 0;
    double                          scale = 0.0;
    std::vector<SCaptureAllocation> allocations;
};

STapeLayout layoutTape(const STapeSpec& tape, SPoint origin, double crossExtent, double gap);
SScene      buildScene(const std::vector<SWorkspaceSpec>& workspaces, int64_t activeWorkspaceID, const SSceneConfig& config);
double      initialPan(const SScene& scene, int64_t activeWorkspaceID, double viewportHeight);
double      clampPan(const SScene& scene, double pan, double viewportHeight);
double      panBy(const SScene& scene, double currentPan, double delta, double viewportHeight);
SHitResult  hitTest(const SScene& scene, SPoint viewportPoint, double pan);
SFocusRef   moveFocus(const SScene& scene, const SFocusRef& current, EFocusDirection direction);
size_t      adjustDestinationColumnIndex(size_t sourceColumn, size_t destinationColumn, bool sourceColumnRemoved);
SDropIntent resolveDrop(const SScene& scene, const SDropSource& source, SPoint viewportPoint, double pan, double edgeFraction = 0.2);
SCapturePlan planCaptureBudget(uint32_t monitorWidth, uint32_t monitorHeight, int multiplier, const std::vector<SCaptureRequest>& requests);

}
