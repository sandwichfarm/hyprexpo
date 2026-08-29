#include "ScrollingMutationTransaction.hpp"

#include <algorithm>
#include <exception>
#include <map>
#include <ranges>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace Hyprexpo::Scrolling {

namespace {

SMutationWorkspace* workspaceFor(SMutationState& state, int64_t workspaceID) {
    const auto workspace = std::ranges::find(state.workspaces, workspaceID, &SMutationWorkspace::workspaceID);
    return workspace == state.workspaces.end() ? nullptr : &*workspace;
}

const SMutationWorkspace* workspaceFor(const SMutationState& state, int64_t workspaceID) {
    const auto workspace = std::ranges::find(state.workspaces, workspaceID, &SMutationWorkspace::workspaceID);
    return workspace == state.workspaces.end() ? nullptr : &*workspace;
}

std::optional<SMutationTarget> targetFor(const SMutationState& state, uint64_t identity) {
    for (const auto& workspace : state.workspaces)
        for (const auto& column : workspace.columns)
            for (const auto& target : column.targets)
                if (target.identity == identity)
                    return target;
    return std::nullopt;
}

std::map<uint64_t, size_t> membershipCounts(const SMutationState& state) {
    std::map<uint64_t, size_t> counts;
    for (const auto& workspace : state.workspaces)
        for (const auto identity : workspace.members)
            ++counts[identity];
    return counts;
}

std::vector<uint64_t> columnOrderWithout(const SMutationWorkspace& workspace, uint64_t movedTarget) {
    std::vector<uint64_t> result;
    for (const auto& column : workspace.columns) {
        if (column.targets.size() == 1 && column.targets.front().identity == movedTarget)
            continue;
        result.push_back(column.identity);
    }
    return result;
}

std::vector<uint64_t> targetOrderWithout(const SMutationColumn& column, uint64_t movedTarget) {
    std::vector<uint64_t> result;
    for (const auto& target : column.targets)
        if (target.identity != movedTarget)
            result.push_back(target.identity);
    return result;
}

void appendUnique(std::vector<std::string>& values, std::string value) {
    if (std::ranges::find(values, value) == values.end())
        values.push_back(std::move(value));
}

template <typename F>
void operation(IMutationOperations& operations, EMutationPhase phase, EMutationStep step, F&& action) {
    operations.checkpoint(phase, step, EFaultWhen::Before);
    std::forward<F>(action)();
    operations.checkpoint(phase, step, EFaultWhen::After);
}

class CInMemoryOperations final : public IMutationOperations {
  public:
    CInMemoryOperations(SMutationState state, std::optional<SFaultInjection> fault) : m_state(std::move(state)), m_fault(fault) {
        for (const auto& workspace : m_state.workspaces)
            for (const auto& column : workspace.columns)
                m_nextColumnIdentity = std::max(m_nextColumnIdentity, column.identity + 1);
    }

    void checkpoint(EMutationPhase phase, EMutationStep step, EFaultWhen when) override {
        const bool injectableHostBoundary = step == EMutationStep::RemoveTarget || step == EMutationStep::ControllerMove || step == EMutationStep::ReResolve ||
            step == EMutationStep::AddTarget || step == EMutationStep::RestoreWidths || step == EMutationStep::RestoreSizes || step == EMutationStep::Recalculate;
        if (phase == EMutationPhase::Apply && injectableHostBoundary && std::ranges::find(m_trace, step) == m_trace.end())
            m_trace.push_back(step);
        if (m_fault && m_fault->phase == phase && m_fault->step == step && m_fault->when == when)
            throw std::runtime_error("injected mutation boundary failure");
        if (m_fault && m_fault->phase == EMutationPhase::Rollback && phase == EMutationPhase::Apply && step == EMutationStep::VerifyPostconditions && when == EFaultWhen::Before)
            throw std::runtime_error("forced apply failure for rollback injection");
    }

    SMutationState snapshotPreState(const SMutationRequest&) override {
        return m_state;
    }

    void removeTarget(const SMutationPlan& plan) override {
        removeFromColumns(*workspaceFor(m_state, plan.request.sourceWorkspaceID), plan.request.targetIdentity);
    }

    void controllerMove(const SMutationPlan& plan, bool reverse) override {
        const int64_t fromID = reverse ? plan.request.destinationWorkspaceID : plan.request.sourceWorkspaceID;
        const int64_t toID   = reverse ? plan.request.sourceWorkspaceID : plan.request.destinationWorkspaceID;
        auto* from = workspaceFor(m_state, fromID);
        auto* to = workspaceFor(m_state, toID);
        if (!from)
            throw std::runtime_error("controller source workspace vanished");
        if (!to) {
            if (!plan.createDestination || reverse)
                throw std::runtime_error("controller destination workspace vanished");
            m_state.workspaces.push_back({.workspaceID = toID, .modelIdentity = static_cast<uint64_t>(1000 + toID), .kind = EMutationWorkspaceKind::Scrolling,
                                          .direction = from->direction, .offset = 0.0, .focusedTargetIdentity = 0, .focusedWindowIdentity = 0,
                                          .columns = {}, .members = {}});
            to = &m_state.workspaces.back();
            from = workspaceFor(m_state, fromID);
        }
        const auto target = targetFor(m_state, plan.request.targetIdentity).value_or(SMutationTarget{.identity = plan.request.targetIdentity});
        removeFromColumns(*from, plan.request.targetIdentity);
        std::erase(from->members, plan.request.targetIdentity);
        to->members.push_back(plan.request.targetIdentity);
        if (to->kind == EMutationWorkspaceKind::Scrolling)
            to->columns.push_back({.identity = m_nextColumnIdentity++, .width = plan.sourceColumnWidth, .targets = {{.identity = target.identity, .windowIdentity = target.windowIdentity, .size = 1.0}}});
        if (reverse)
            ++m_reverseMoveCount;
        else
            ++m_controllerMoveCount;
    }

    void reResolve(const SMutationPlan&, bool) override {
        // The in-memory model has no borrowed references; this boundary remains fault injectable.
    }

    void addTarget(const SMutationPlan& plan) override {
        auto* destination = workspaceFor(m_state, plan.request.destinationWorkspaceID);
        if (!destination || destination->kind != EMutationWorkspaceKind::Scrolling)
            throw std::runtime_error("native destination model unavailable");

        const auto target = targetFor(m_beforeSnapshot, plan.request.targetIdentity).value();
        removeFromColumns(*destination, plan.request.targetIdentity);
        if (plan.request.placement == EColumnPlacement::None) {
            destination->columns.push_back({.identity = m_nextColumnIdentity++, .width = plan.sourceColumnWidth,
                                            .targets = {{.identity = target.identity, .windowIdentity = target.windowIdentity, .size = 1.0}}});
            return;
        }
        if (plan.request.placement == EColumnPlacement::Existing) {
            if (plan.request.destinationColumnIndex >= destination->columns.size())
                throw std::runtime_error("destination column index is stale");
            auto& targets = destination->columns[plan.request.destinationColumnIndex].targets;
            if (plan.request.destinationRowIndex > targets.size())
                throw std::runtime_error("destination row index is stale");
            targets.insert(targets.begin() + plan.request.destinationRowIndex, target);
            return;
        }
        if (plan.request.destinationColumnIndex > destination->columns.size())
            throw std::runtime_error("new column index is stale");
        destination->columns.insert(destination->columns.begin() + plan.request.destinationColumnIndex,
                                    {.identity = m_nextColumnIdentity++, .width = plan.sourceColumnWidth,
                                     .targets = {{.identity = target.identity, .windowIdentity = target.windowIdentity, .size = 1.0}}});
    }

    void restorePreState(const SMutationState& before, const SMutationPlan&) override {
        m_state = before;
    }

    void restoreWidths(const SMutationState& before, const SMutationPlan& plan, bool rollback) override {
        const auto& reference = rollback ? before : m_beforeSnapshot;
        for (auto& workspace : m_state.workspaces) {
            const auto* oldWorkspace = workspaceFor(reference, workspace.workspaceID);
            for (auto& column : workspace.columns) {
                if (oldWorkspace) {
                    const auto oldColumn = std::ranges::find(oldWorkspace->columns, column.identity, &SMutationColumn::identity);
                    if (oldColumn != oldWorkspace->columns.end()) {
                        column.width = oldColumn->width;
                        continue;
                    }
                }
                const bool containsMoved = std::ranges::any_of(column.targets, [&](const auto& target) { return target.identity == plan.request.targetIdentity; });
                if (containsMoved)
                    column.width = plan.sourceColumnWidth;
            }
        }
    }

    void restoreSizes(const SMutationState& before, const SMutationPlan& plan, bool rollback) override {
        const auto& reference = rollback ? before : m_beforeSnapshot;
        std::unordered_map<uint64_t, double> sizes;
        for (const auto& workspace : reference.workspaces)
            for (const auto& column : workspace.columns)
                for (const auto& target : column.targets)
                    sizes[target.identity] = target.size;
        for (auto& workspace : m_state.workspaces)
            for (auto& column : workspace.columns)
                for (auto& target : column.targets)
                    if (const auto size = sizes.find(target.identity); size != sizes.end())
                        target.size = column.targets.size() == 1 && target.identity == plan.request.targetIdentity ? 1.0 : size->second;
    }

    void recalculate(const SMutationPlan&, bool) override {}

    SMutationState snapshotPostState(const SMutationPlan&, bool) override {
        return m_state;
    }

    void setBeforeSnapshot(SMutationState state) {
        m_beforeSnapshot = std::move(state);
    }

    static void removeFromColumns(SMutationWorkspace& workspace, uint64_t identity) {
        for (auto& column : workspace.columns)
            std::erase_if(column.targets, [&](const auto& target) { return target.identity == identity; });
        std::erase_if(workspace.columns, [](const auto& column) { return column.targets.empty(); });
    }

    SMutationState             m_state;
    SMutationState             m_beforeSnapshot;
    std::optional<SFaultInjection> m_fault;
    std::vector<EMutationStep> m_trace;
    uint64_t                   m_nextColumnIdentity = 10000;
    size_t                     m_controllerMoveCount = 0;
    size_t                     m_reverseMoveCount = 0;
};

}

SMutationPlan buildMutationPlan(const SMutationState& before, const SMutationRequest& request) {
    SMutationPlan plan;
    plan.createDestination = request.createDestination;
    plan.request = request;
    if (request.kind == EDropKind::Invalid || request.targetIdentity == 0 || request.sourceWorkspaceID <= 0 || request.destinationWorkspaceID <= 0) {
        plan.error = "invalid mutation request";
        return plan;
    }

    size_t matches = 0;
    for (size_t workspaceIndex = 0; workspaceIndex < before.workspaces.size(); ++workspaceIndex) {
        const auto& workspace = before.workspaces[workspaceIndex];
        for (size_t columnIndex = 0; columnIndex < workspace.columns.size(); ++columnIndex) {
            const auto& column = workspace.columns[columnIndex];
            for (size_t rowIndex = 0; rowIndex < column.targets.size(); ++rowIndex) {
                const auto& target = column.targets[rowIndex];
                if (target.identity != request.targetIdentity)
                    continue;
                ++matches;
                plan.sourceWorkspaceIndex = workspaceIndex;
                plan.sourceColumnIndex = columnIndex;
                plan.sourceRowIndex = rowIndex;
                plan.sourceColumnIdentity = column.identity;
                plan.sourceColumnWidth = column.width;
                plan.sourceTargetSize = target.size;
                if (target.group || target.fullscreen)
                    plan.error = "grouped or fullscreen targets cannot be transactionally moved";
            }
        }
    }
    if (matches != 1 || plan.sourceWorkspaceIndex >= before.workspaces.size() || before.workspaces[plan.sourceWorkspaceIndex].workspaceID != request.sourceWorkspaceID) {
        plan.error = "source target membership is not exact";
        return plan;
    }
    if (!plan.error.empty())
        return plan;

    const auto destination = std::ranges::find(before.workspaces, request.destinationWorkspaceID, &SMutationWorkspace::workspaceID);
    if (destination != before.workspaces.end())
        plan.destinationWorkspaceIndex = static_cast<size_t>(std::distance(before.workspaces.begin(), destination));
    else if (!request.createDestination) {
        plan.error = "destination workspace is unavailable";
        return plan;
    }

    plan.noOp = request.kind == EDropKind::NoOp;
    plan.crossWorkspace = request.sourceWorkspaceID != request.destinationWorkspaceID;
    plan.mixedFallback = request.kind == EDropKind::MixedFallback;
    if (plan.mixedFallback && (destination == before.workspaces.end() || destination->kind != EMutationWorkspaceKind::Mixed)) {
        plan.error = "mixed fallback destination is not mixed";
        return plan;
    }
    if (!plan.mixedFallback && destination != before.workspaces.end() && destination->kind != EMutationWorkspaceKind::Scrolling) {
        plan.error = "native scrolling destination is unavailable";
        return plan;
    }
    if (!plan.noOp && !plan.crossWorkspace && request.kind != EDropKind::ExistingColumn && request.kind != EDropKind::NewColumnBefore && request.kind != EDropKind::NewColumnAfter) {
        plan.error = "same-workspace mutation kind is unsupported";
        return plan;
    }
    plan.valid = true;
    return plan;
}

std::vector<std::string> verifyPostconditions(const SMutationState& before, const SMutationState& after, const SMutationPlan& plan, bool rollback) {
    std::vector<std::string> violations;
    if (rollback) {
        if (before != after)
            violations.push_back("rollback.exact-state");
        return violations;
    }

    const auto beforeCounts = membershipCounts(before);
    const auto afterCounts = membershipCounts(after);
    if (beforeCounts.size() != afterCounts.size())
        appendUnique(violations, "membership.exact-once");
    for (const auto& [identity, count] : beforeCounts) {
        const auto found = afterCounts.find(identity);
        if (count != 1 || found == afterCounts.end() || found->second != 1)
            appendUnique(violations, "membership.exact-once");
    }
    for (const auto& [identity, count] : afterCounts)
        if (!beforeCounts.contains(identity) || count != 1)
            appendUnique(violations, "membership.exact-once");

    for (const auto& oldWorkspace : before.workspaces) {
        const auto* current = workspaceFor(after, oldWorkspace.workspaceID);
        if (!current)
            continue;
        if (oldWorkspace.direction != current->direction || oldWorkspace.offset != current->offset)
            appendUnique(violations, "controller.direction-offset");
        if (oldWorkspace.focusedTargetIdentity != current->focusedTargetIdentity || oldWorkspace.focusedWindowIdentity != current->focusedWindowIdentity)
            appendUnique(violations, "focus.identity");
        if (columnOrderWithout(oldWorkspace, plan.request.targetIdentity) != columnOrderWithout(*current, plan.request.targetIdentity))
            appendUnique(violations, "unaffected.order");
        for (const auto& oldColumn : oldWorkspace.columns) {
            const auto currentColumn = std::ranges::find(current->columns, oldColumn.identity, &SMutationColumn::identity);
            const bool movedOnly = oldColumn.targets.size() == 1 && oldColumn.targets.front().identity == plan.request.targetIdentity;
            if (currentColumn == current->columns.end()) {
                if (!movedOnly)
                    appendUnique(violations, "unaffected.order");
                continue;
            }
            if (oldColumn.width != currentColumn->width)
                appendUnique(violations, "unaffected.width");
            if (targetOrderWithout(oldColumn, plan.request.targetIdentity) != targetOrderWithout(*currentColumn, plan.request.targetIdentity))
                appendUnique(violations, "unaffected.order");
            for (const auto& oldTarget : oldColumn.targets) {
                if (oldTarget.identity == plan.request.targetIdentity)
                    continue;
                const auto currentTarget = std::ranges::find(currentColumn->targets, oldTarget.identity, &SMutationTarget::identity);
                if (currentTarget == currentColumn->targets.end() || currentTarget->size != oldTarget.size)
                    appendUnique(violations, "unaffected.size");
            }
        }
    }

    const auto* destination = workspaceFor(after, plan.request.destinationWorkspaceID);
    if (!destination || std::ranges::count(destination->members, plan.request.targetIdentity) != 1)
        appendUnique(violations, "moved.expected-workspace");
    if (destination && destination->kind == EMutationWorkspaceKind::Scrolling && !plan.mixedFallback) {
        size_t occurrences = 0;
        std::optional<std::pair<size_t, size_t>> location;
        for (size_t columnIndex = 0; columnIndex < destination->columns.size(); ++columnIndex)
            for (size_t rowIndex = 0; rowIndex < destination->columns[columnIndex].targets.size(); ++rowIndex)
                if (destination->columns[columnIndex].targets[rowIndex].identity == plan.request.targetIdentity) {
                    ++occurrences;
                    location = {{columnIndex, rowIndex}};
                }
        if (occurrences != 1)
            appendUnique(violations, "topology.exact-once");
        if (plan.request.placement != EColumnPlacement::None &&
            (!location || location->first != plan.request.destinationColumnIndex || location->second != plan.request.destinationRowIndex))
            appendUnique(violations, "moved.expected-location");
    }
    return violations;
}

SMutationResult executeMutation(const SMutationRequest& request, IMutationOperations& operations) {
    SMutationResult result;
    bool controllerMoved = false;
    try {
        operation(operations, EMutationPhase::Apply, EMutationStep::SnapshotPreState, [&] { result.before = operations.snapshotPreState(request); });
        result.plan = buildMutationPlan(result.before, request);
        if (!result.plan.valid) {
            result.error = result.plan.error;
            return result;
        }
        if (result.plan.noOp) {
            result.after = result.before;
            result.outcome = EMutationOutcome::Committed;
            return result;
        }
        if (result.plan.crossWorkspace) {
            operation(operations, EMutationPhase::Apply, EMutationStep::ControllerMove, [&] { operations.controllerMove(result.plan, false); });
            controllerMoved = true;
            operation(operations, EMutationPhase::Apply, EMutationStep::ReResolve, [&] { operations.reResolve(result.plan, false); });
        } else {
            operation(operations, EMutationPhase::Apply, EMutationStep::RemoveTarget, [&] { operations.removeTarget(result.plan); });
        }
        if (!result.plan.mixedFallback)
            operation(operations, EMutationPhase::Apply, EMutationStep::AddTarget, [&] { operations.addTarget(result.plan); });
        operation(operations, EMutationPhase::Apply, EMutationStep::RestoreWidths, [&] { operations.restoreWidths(result.before, result.plan, false); });
        operation(operations, EMutationPhase::Apply, EMutationStep::RestoreSizes, [&] { operations.restoreSizes(result.before, result.plan, false); });
        operation(operations, EMutationPhase::Apply, EMutationStep::Recalculate, [&] { operations.recalculate(result.plan, false); });
        operation(operations, EMutationPhase::Apply, EMutationStep::SnapshotPostState, [&] { result.after = operations.snapshotPostState(result.plan, false); });
        operation(operations, EMutationPhase::Apply, EMutationStep::VerifyPostconditions,
                  [&] { result.violatedInvariantIDs = verifyPostconditions(result.before, result.after, result.plan, false); });
        if (!result.violatedInvariantIDs.empty())
            throw std::runtime_error("mutation postconditions failed");
        result.outcome = EMutationOutcome::Committed;
        return result;
    } catch (const std::exception& error) {
        result.error = error.what();
    } catch (...) {
        result.error = "unknown mutation exception";
    }

    try {
        if (controllerMoved) {
            operation(operations, EMutationPhase::Rollback, EMutationStep::ReverseControllerMove, [&] { operations.controllerMove(result.plan, true); });
            operation(operations, EMutationPhase::Rollback, EMutationStep::ReResolve, [&] { operations.reResolve(result.plan, true); });
        }
        operation(operations, EMutationPhase::Rollback, EMutationStep::RestorePreState, [&] { operations.restorePreState(result.before, result.plan); });
        operation(operations, EMutationPhase::Rollback, EMutationStep::RestoreWidths, [&] { operations.restoreWidths(result.before, result.plan, true); });
        operation(operations, EMutationPhase::Rollback, EMutationStep::RestoreSizes, [&] { operations.restoreSizes(result.before, result.plan, true); });
        operation(operations, EMutationPhase::Rollback, EMutationStep::Recalculate, [&] { operations.recalculate(result.plan, true); });
        operation(operations, EMutationPhase::Rollback, EMutationStep::SnapshotPostState, [&] { result.after = operations.snapshotPostState(result.plan, true); });
        operation(operations, EMutationPhase::Rollback, EMutationStep::VerifyPostconditions,
                  [&] { result.violatedInvariantIDs = verifyPostconditions(result.before, result.after, result.plan, true); });
        if (!result.violatedInvariantIDs.empty())
            throw std::runtime_error("rollback postconditions failed");
        result.outcome = EMutationOutcome::RolledBack;
        return result;
    } catch (const std::exception& error) {
        if (result.error.empty())
            result.error = error.what();
        else
            result.error += "; rollback: " + std::string{error.what()};
    } catch (...) {
        result.error += "; rollback: unknown exception";
    }
    if (result.violatedInvariantIDs.empty())
        result.violatedInvariantIDs.push_back("rollback.boundary-failure");
    result.outcome = EMutationOutcome::RollbackFailed;
    return result;
}

SMutationSimulation simulateMutation(SMutationState initial, const SMutationRequest& request, std::optional<SFaultInjection> fault) {
    CInMemoryOperations operations{std::move(initial), fault};
    operations.setBeforeSnapshot(operations.m_state);
    auto result = executeMutation(request, operations);
    return {.result = std::move(result), .state = std::move(operations.m_state), .trace = std::move(operations.m_trace),
            .controllerMoveCount = operations.m_controllerMoveCount, .reverseMoveCount = operations.m_reverseMoveCount};
}

}
