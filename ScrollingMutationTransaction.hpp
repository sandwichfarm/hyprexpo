#pragma once

#include "ScrollingOverviewLogic.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Hyprexpo::Scrolling {

enum class EMutationWorkspaceKind {
    Scrolling,
    Mixed,
};

struct SMutationTarget {
    uint64_t identity = 0;
    uint64_t windowIdentity = 0;
    double   size = 0.0;
    bool     group = false;
    bool     fullscreen = false;

    bool operator==(const SMutationTarget&) const = default;
};

struct SMutationColumn {
    uint64_t                     identity = 0;
    double                       width = 0.0;
    std::vector<SMutationTarget> targets;

    bool operator==(const SMutationColumn&) const = default;
};

struct SMutationWorkspace {
    int64_t                       workspaceID = 0;
    uint64_t                      modelIdentity = 0;
    EMutationWorkspaceKind        kind = EMutationWorkspaceKind::Scrolling;
    std::string                   direction;
    double                        offset = 0.0;
    uint64_t                      focusedTargetIdentity = 0;
    uint64_t                      focusedWindowIdentity = 0;
    std::vector<SMutationColumn>  columns;
    std::vector<uint64_t>         members;

    bool operator==(const SMutationWorkspace&) const = default;
};

struct SMutationState {
    std::vector<SMutationWorkspace> workspaces;

    bool operator==(const SMutationState&) const = default;
};

struct SMutationRequest {
    std::string      requestID = {};
    uint64_t         sessionGeneration = 0;
    uint64_t         targetIdentity = 0;
    int64_t          sourceWorkspaceID = 0;
    int64_t          destinationWorkspaceID = 0;
    EDropKind        kind = EDropKind::Invalid;
    EColumnPlacement placement = EColumnPlacement::None;
    size_t           destinationColumnIndex = 0;
    size_t           destinationRowIndex = 0;
    bool             createDestination = false;
};

struct SMutationPlan {
    bool              valid = false;
    bool              noOp = false;
    bool              crossWorkspace = false;
    bool              mixedFallback = false;
    bool              createDestination = false;
    std::string       error;
    SMutationRequest  request;
    size_t            sourceWorkspaceIndex = 0;
    size_t            sourceColumnIndex = 0;
    size_t            sourceRowIndex = 0;
    uint64_t          sourceColumnIdentity = 0;
    double            sourceColumnWidth = 0.0;
    double            sourceTargetSize = 0.0;
    std::optional<size_t> destinationWorkspaceIndex;
};

enum class EMutationOutcome {
    Committed,
    RolledBack,
    RollbackFailed,
    Rejected,
};

enum class EMutationPhase {
    Apply,
    Rollback,
};

enum class EFaultWhen {
    Before,
    After,
};

enum class EMutationStep {
    SnapshotPreState,
    RemoveTarget,
    ControllerMove,
    ReResolve,
    AddTarget,
    RestoreWidths,
    RestoreSizes,
    Recalculate,
    SnapshotPostState,
    VerifyPostconditions,
    ReverseControllerMove,
    RestorePreState,
};

struct SFaultInjection {
    EMutationPhase phase = EMutationPhase::Apply;
    EMutationStep  step = EMutationStep::SnapshotPreState;
    EFaultWhen     when = EFaultWhen::Before;
};

struct SMutationResult {
    EMutationOutcome         outcome = EMutationOutcome::Rejected;
    SMutationPlan            plan;
    SMutationState           before;
    SMutationState           after;
    std::vector<std::string> violatedInvariantIDs;
    std::string              error;
};

class IMutationOperations {
  public:
    virtual ~IMutationOperations() = default;

    virtual void checkpoint(EMutationPhase phase, EMutationStep step, EFaultWhen when) = 0;
    virtual SMutationState snapshotPreState(const SMutationRequest& request) = 0;
    virtual void removeTarget(const SMutationPlan& plan) = 0;
    virtual void controllerMove(const SMutationPlan& plan, bool reverse) = 0;
    virtual void reResolve(const SMutationPlan& plan, bool rollback) = 0;
    virtual void addTarget(const SMutationPlan& plan) = 0;
    virtual void restorePreState(const SMutationState& before, const SMutationPlan& plan) = 0;
    virtual void restoreWidths(const SMutationState& before, const SMutationPlan& plan, bool rollback) = 0;
    virtual void restoreSizes(const SMutationState& before, const SMutationPlan& plan, bool rollback) = 0;
    virtual void recalculate(const SMutationPlan& plan, bool rollback) = 0;
    virtual SMutationState snapshotPostState(const SMutationPlan& plan, bool rollback) = 0;
};

SMutationPlan              buildMutationPlan(const SMutationState& before, const SMutationRequest& request);
std::vector<std::string>   verifyPostconditions(const SMutationState& before, const SMutationState& after, const SMutationPlan& plan, bool rollback);
SMutationResult            executeMutation(const SMutationRequest& request, IMutationOperations& operations);

struct SMutationSimulation {
    SMutationResult            result;
    SMutationState             state;
    std::vector<SFaultInjection> boundaries;
    size_t                     controllerMoveCount = 0;
    size_t                     reverseMoveCount = 0;
};

SMutationSimulation simulateMutation(SMutationState initial, const SMutationRequest& request, std::optional<SFaultInjection> fault = std::nullopt);
int64_t             nextUnusedOrdinaryWorkspaceID(const std::vector<int64_t>& workspaceIDs);
std::vector<double> expectedCommittedTargetSizes(const SMutationState& before, const SMutationPlan& plan, int64_t workspaceID, uint64_t columnIdentity,
                                                 const std::vector<uint64_t>& targetIdentities);

}
