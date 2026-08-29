#include "ScrollingDiagnostics.hpp"

#include "ScrollingRequestId.hpp"
#include "globals.hpp"

#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace Hyprexpo::Scrolling {

namespace {

constexpr std::string_view ACTIVE_SELECTOR = "active";
constexpr std::string_view WORKSPACE_PREFIX = "workspace:";

std::string quoted(std::string_view value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char c : value) {
        switch (c) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (c < 0x20)
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
                else
                    output << static_cast<char>(c);
        }
    }
    output << '"';
    return output.str();
}

std::string pointerValue(uintptr_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << value;
    return output.str();
}

std::string boxJson(const CBox& box) {
    std::ostringstream output;
    output << "{\"x\":" << box.x << ",\"y\":" << box.y << ",\"w\":" << box.w << ",\"h\":" << box.h << '}';
    return output.str();
}

bool sameBox(const CBox& left, const CBox& right) {
    return left.x == right.x && left.y == right.y && left.w == right.w && left.h == right.h;
}

bool sameOrder(const SWorkspaceSnapshot& before, const SWorkspaceSnapshot& after) {
    if (before.columns.size() != after.columns.size())
        return false;
    for (size_t columnIndex = 0; columnIndex < before.columns.size(); ++columnIndex) {
        const auto& left = before.columns[columnIndex];
        const auto& right = after.columns[columnIndex];
        if (left.fingerprint != right.fingerprint || left.targets.size() != right.targets.size())
            return false;
        for (size_t rowIndex = 0; rowIndex < left.targets.size(); ++rowIndex) {
            if (left.targets[rowIndex].targetFingerprint != right.targets[rowIndex].targetFingerprint)
                return false;
        }
    }
    return true;
}

bool sameWidths(const SWorkspaceSnapshot& before, const SWorkspaceSnapshot& after) {
    if (before.columns.size() != after.columns.size())
        return false;
    for (size_t index = 0; index < before.columns.size(); ++index) {
        if (before.columns[index].width != after.columns[index].width || before.columns[index].primarySize != after.columns[index].primarySize)
            return false;
    }
    return true;
}

bool sameMembership(const SWorkspaceSnapshot& before, const SWorkspaceSnapshot& after) {
    if (!sameOrder(before, after) || before.layoutTargets.size() != after.layoutTargets.size())
        return false;
    for (size_t index = 0; index < before.layoutTargets.size(); ++index) {
        if (before.layoutTargets[index].targetFingerprint != after.layoutTargets[index].targetFingerprint ||
            before.layoutTargets[index].windowFingerprint != after.layoutTargets[index].windowFingerprint)
            return false;
    }
    return true;
}

bool sameSizes(const SWorkspaceSnapshot& before, const SWorkspaceSnapshot& after) {
    if (before.columns.size() != after.columns.size())
        return false;
    for (size_t columnIndex = 0; columnIndex < before.columns.size(); ++columnIndex) {
        const auto& left = before.columns[columnIndex];
        const auto& right = after.columns[columnIndex];
        if (left.targets.size() != right.targets.size())
            return false;
        for (size_t rowIndex = 0; rowIndex < left.targets.size(); ++rowIndex) {
            const auto& leftTarget = left.targets[rowIndex];
            const auto& rightTarget = right.targets[rowIndex];
            if (leftTarget.proportion != rightTarget.proportion || !sameBox(leftTarget.layoutBox, rightTarget.layoutBox))
                return false;
        }
    }
    return true;
}

bool sameSpecialState(const SWorkspaceSnapshot& before, const SWorkspaceSnapshot& after) {
    if (before.layoutTargets.size() != after.layoutTargets.size())
        return false;
    for (size_t index = 0; index < before.layoutTargets.size(); ++index) {
        const auto& left = before.layoutTargets[index];
        const auto& right = after.layoutTargets[index];
        if (left.group != right.group || left.floating != right.floating || left.fullscreen != right.fullscreen || left.pinned != right.pinned || left.visible != right.visible ||
            !sameBox(left.layoutBox, right.layoutBox))
            return false;
    }
    return true;
}

std::string targetJson(const STargetSnapshot& target) {
    std::ostringstream output;
    output << "{\"row\":" << target.rowIndex << ",\"proportion\":" << target.proportion
           << ",\"targetFingerprint\":" << quoted(pointerValue(target.targetFingerprint))
           << ",\"windowFingerprint\":" << quoted(pointerValue(target.windowFingerprint))
           << ",\"windowStableId\":" << target.windowStableID << ",\"layoutBox\":" << boxJson(target.layoutBox)
           << ",\"visible\":" << (target.visible ? "true" : "false") << ",\"group\":" << (target.group ? "true" : "false")
           << ",\"floating\":" << (target.floating ? "true" : "false") << ",\"fullscreen\":" << (target.fullscreen ? "true" : "false")
           << ",\"pinned\":" << (target.pinned ? "true" : "false") << '}';
    return output.str();
}

std::string columnsJson(const SWorkspaceSnapshot& snapshot) {
    std::ostringstream output;
    output << '[';
    for (size_t columnIndex = 0; columnIndex < snapshot.columns.size(); ++columnIndex) {
        const auto& column = snapshot.columns[columnIndex];
        if (columnIndex)
            output << ',';
        output << "{\"index\":" << column.index << ",\"fingerprint\":" << quoted(pointerValue(column.fingerprint)) << ",\"width\":" << column.width
               << ",\"primaryStart\":" << column.primaryStart << ",\"primarySize\":" << column.primarySize << ",\"visible\":" << (column.visible ? "true" : "false")
               << ",\"targets\":[";
        for (size_t targetIndex = 0; targetIndex < column.targets.size(); ++targetIndex) {
            if (targetIndex)
                output << ',';
            output << targetJson(column.targets[targetIndex]);
        }
        output << "]}";
    }
    output << ']';
    return output.str();
}

PHLWORKSPACE resolveWorkspace(const std::string& selector) {
    if (selector == ACTIVE_SELECTOR) {
        const auto focus = Desktop::focusState();
        const auto monitor = focus ? focus->monitor() : PHLMONITOR{};
        return monitor ? monitor->m_activeWorkspace : PHLWORKSPACE{};
    }

    int64_t workspaceID = 0;
    const auto value = std::string_view{selector}.substr(WORKSPACE_PREFIX.size());
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), workspaceID);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        return {};
    for (const auto& workspace : State::workspaceState()->workspacesCopy()) {
        if (workspace && workspace->m_id == workspaceID)
            return workspace;
    }
    return {};
}

std::string resultError(const SSnapshotResult& result) {
    return snapshotFailureName(result.failure) + (result.error.empty() ? "" : ": " + result.error);
}

std::string mutationStateSummary(const SMutationState& state) {
    size_t columns = 0;
    size_t targets = 0;
    size_t members = 0;
    for (const auto& workspace : state.workspaces) {
        columns += workspace.columns.size();
        members += workspace.members.size();
        for (const auto& column : workspace.columns)
            targets += column.targets.size();
    }
    return std::format("{{\"workspaces\":{},\"columns\":{},\"targets\":{},\"members\":{}}}", state.workspaces.size(), columns, targets, members);
}

uint64_t mutationStateHash(const SMutationState& state) {
    std::ostringstream material;
    for (const auto& workspace : state.workspaces) {
        material << workspace.workspaceID << ':' << workspace.modelIdentity << ':' << workspace.direction << ':' << workspace.offset << ':'
                 << workspace.focusedTargetIdentity << ':' << workspace.focusedWindowIdentity << '|';
        for (const auto& column : workspace.columns) {
            material << column.identity << ':' << column.width << '[';
            for (const auto& target : column.targets)
                material << target.identity << ':' << target.windowIdentity << ':' << target.size << ',';
            material << ']';
        }
        material << '{';
        for (const auto member : workspace.members)
            material << member << ',';
        material << '}';
    }
    uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char byte : material.str()) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace

SDiagnosticRequest parseDiagnosticRequest(const std::string& argument) {
    std::istringstream input{argument};
    SDiagnosticRequest request;
    std::string extra;
    if (!(input >> request.requestID >> request.selector) || (input >> extra)) {
        request.error = "expected REQUEST_ID and active|workspace:ID";
        return request;
    }
    if (!validRequestID(request.requestID)) {
        request.error = "request ID must be 1-64 ASCII letters, digits, dot, underscore, or dash";
        return request;
    }
    if (request.selector != ACTIVE_SELECTOR) {
        if (!request.selector.starts_with(WORKSPACE_PREFIX) || request.selector.size() == WORKSPACE_PREFIX.size()) {
            request.error = "selector must be active or workspace:ID";
            return request;
        }
        int64_t workspaceID = 0;
        const auto value = std::string_view{request.selector}.substr(WORKSPACE_PREFIX.size());
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), workspaceID);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || workspaceID <= 0) {
            request.error = "workspace selector ID must be a positive integer";
            return request;
        }
    }
    request.valid = true;
    return request;
}

bool snapshotsEquivalent(const SWorkspaceSnapshot& before, const SWorkspaceSnapshot& after) {
    return before.workspaceID == after.workspaceID && before.monitorID == after.monitorID && before.algorithmFingerprint == after.algorithmFingerprint && before.dataFingerprint == after.dataFingerprint &&
        before.direction == after.direction && before.offset == after.offset && before.activeWorkspaceID == after.activeWorkspaceID && before.focusedWindowFingerprint == after.focusedWindowFingerprint &&
        sameOrder(before, after) && sameWidths(before, after) && sameMembership(before, after) && sameSizes(before, after) && sameSpecialState(before, after);
}

SDiagnosticEmission buildReadDiagnostic(const std::string& argument) {
    const auto request = parseDiagnosticRequest(argument);
    if (!request.valid)
        return {.validRequest = false, .success = false, .requestID = request.requestID, .error = request.error, .json = {}};

    const auto workspace = resolveWorkspace(request.selector);
    const auto before = snapshotWorkspace(workspace);
    const auto after = snapshotWorkspace(workspace);
    const bool snapshotSuccess = before.success() && after.success();
    const bool topologyEqual = snapshotSuccess && snapshotsEquivalent(*before.snapshot, *after.snapshot);
    const bool directionEqual = snapshotSuccess && before.snapshot->direction == after.snapshot->direction;
    const bool offsetEqual = snapshotSuccess && before.snapshot->offset == after.snapshot->offset;
    const bool orderEqual = snapshotSuccess && sameOrder(*before.snapshot, *after.snapshot);
    const bool widthsEqual = snapshotSuccess && sameWidths(*before.snapshot, *after.snapshot);
    const bool membershipEqual = snapshotSuccess && sameMembership(*before.snapshot, *after.snapshot);
    const bool sizesEqual = snapshotSuccess && sameSizes(*before.snapshot, *after.snapshot);
    const bool specialStateEqual = snapshotSuccess && sameSpecialState(*before.snapshot, *after.snapshot);
    const bool algorithmEqual = snapshotSuccess && before.snapshot->algorithmFingerprint == after.snapshot->algorithmFingerprint;
    const bool dataEqual = snapshotSuccess && before.snapshot->dataFingerprint == after.snapshot->dataFingerprint;
    const bool activeWorkspaceEqual = snapshotSuccess && before.snapshot->activeWorkspaceID == after.snapshot->activeWorkspaceID;
    const bool focusEqual = snapshotSuccess && before.snapshot->focusedWindowFingerprint == after.snapshot->focusedWindowFingerprint;
    const auto version = HyprlandAPI::getHyprlandVersion(PHANDLE);
    const auto* snapshot = before.snapshot ? &*before.snapshot : nullptr;
    const std::string error = !before.success() ? resultError(before) : !after.success() ? resultError(after) : !topologyEqual ? "read-only before/after topology mismatch" : "";

    std::ostringstream output;
    output << "{\"schema\":1,\"kind\":\"snapshot\",\"requestId\":" << quoted(request.requestID)
           << ",\"sessionGeneration\":0,\"marker\":null,\"hyprlandVersion\":" << quoted(version.tag) << ",\"runtimeHash\":" << quoted(__hyprland_api_get_hash())
           << ",\"clientHash\":" << quoted(__hyprland_api_get_client_hash()) << ",\"monitorId\":" << (snapshot ? snapshot->monitorID : 0)
           << ",\"workspaceId\":" << (snapshot ? snapshot->workspaceID : 0) << ",\"algorithmFingerprint\":" << quoted(pointerValue(snapshot ? snapshot->algorithmFingerprint : 0))
           << ",\"dataFingerprint\":" << quoted(pointerValue(snapshot ? snapshot->dataFingerprint : 0)) << ",\"direction\":" << quoted(snapshot ? snapshot->direction : "")
           << ",\"offsetBefore\":" << (before.snapshot ? before.snapshot->offset : 0.0) << ",\"offsetAfter\":" << (after.snapshot ? after.snapshot->offset : 0.0)
           << ",\"activeWorkspaceBefore\":" << (before.snapshot ? before.snapshot->activeWorkspaceID : 0) << ",\"activeWorkspaceAfter\":" << (after.snapshot ? after.snapshot->activeWorkspaceID : 0)
           << ",\"focusedWindowBefore\":" << quoted(pointerValue(before.snapshot ? before.snapshot->focusedWindowFingerprint : 0))
           << ",\"focusedWindowAfter\":" << quoted(pointerValue(after.snapshot ? after.snapshot->focusedWindowFingerprint : 0))
           << ",\"columns\":" << (snapshot ? columnsJson(*snapshot) : "[]") << ",\"layoutTargets\":[";
    if (snapshot) {
        for (size_t index = 0; index < snapshot->layoutTargets.size(); ++index) {
            if (index)
                output << ',';
            output << targetJson(snapshot->layoutTargets[index]);
        }
    }
    output << "],\"captureStatus\":\"not-requested\",\"retainedFramebuffer\":false,\"physicalPresentationBox\":null,\"logicalCropBox\":null,\"pixelEvidence\":null"
           << ",\"pendingGeneration\":0,\"pendingFramebuffer\":false,\"pendingTexture\":false,\"pendingOverlay\":false,\"pendingPassCount\":0,\"acknowledged\":false,\"cleanupComplete\":true"
           << ",\"topologyEqual\":" << (topologyEqual ? "true" : "false") << ",\"directionEqual\":" << (directionEqual ? "true" : "false")
           << ",\"offsetEqual\":" << (offsetEqual ? "true" : "false") << ",\"orderEqual\":" << (orderEqual ? "true" : "false")
           << ",\"widthsEqual\":" << (widthsEqual ? "true" : "false") << ",\"membershipEqual\":" << (membershipEqual ? "true" : "false")
           << ",\"sizesEqual\":" << (sizesEqual ? "true" : "false") << ",\"specialStateEqual\":" << (specialStateEqual ? "true" : "false")
           << ",\"algorithmEqual\":" << (algorithmEqual ? "true" : "false") << ",\"dataEqual\":" << (dataEqual ? "true" : "false")
           << ",\"activeWorkspaceEqual\":" << (activeWorkspaceEqual ? "true" : "false") << ",\"focusEqual\":" << (focusEqual ? "true" : "false")
           << ",\"mutationOutcome\":\"not-attempted\",\"rollbackStatus\":\"not-required\""
           << ",\"error\":" << quoted(error) << ",\"status\":" << quoted(topologyEqual ? "PASS" : "FAIL") << '}';
    return {.validRequest = true, .success = topologyEqual, .requestID = request.requestID, .error = error, .json = output.str()};
}

std::string mutationDiagnosticJson(const SMutationResult& result) {
    const auto outcome = [&]() -> std::string_view {
        switch (result.outcome) {
            case EMutationOutcome::Committed: return "committed";
            case EMutationOutcome::RolledBack: return "rolled-back";
            case EMutationOutcome::RollbackFailed: return "rollback-failed";
            case EMutationOutcome::Rejected: return "rejected";
        }
        return "rejected";
    }();
    std::ostringstream output;
    output << "{\"schema\":1,\"kind\":\"mutation\",\"requestId\":" << quoted(result.plan.request.requestID)
           << ",\"sessionGeneration\":" << result.plan.request.sessionGeneration << ",\"mutationOutcome\":" << quoted(outcome)
           << ",\"rollbackStatus\":" << quoted(result.outcome == EMutationOutcome::RolledBack ? "restored" : result.outcome == EMutationOutcome::RollbackFailed ? "failed" : "not-required")
           << ",\"sourceWorkspaceId\":" << result.plan.request.sourceWorkspaceID
           << ",\"destinationWorkspaceId\":" << result.plan.request.destinationWorkspaceID
           << ",\"targetIdentity\":" << quoted(pointerValue(result.plan.request.targetIdentity))
           << ",\"beforeSummary\":" << mutationStateSummary(result.before) << ",\"afterSummary\":" << mutationStateSummary(result.after)
           << ",\"beforeHash\":" << quoted(pointerValue(mutationStateHash(result.before)))
           << ",\"afterHash\":" << quoted(pointerValue(mutationStateHash(result.after))) << ",\"violatedInvariantIDs\":[";
    for (size_t index = 0; index < result.violatedInvariantIDs.size(); ++index) {
        if (index)
            output << ',';
        output << quoted(result.violatedInvariantIDs[index]);
    }
    output << "],\"error\":" << quoted(result.error) << ",\"status\":"
           << quoted(result.outcome == EMutationOutcome::Committed || result.outcome == EMutationOutcome::RolledBack ? "PASS" : "FAIL") << '}';
    return output.str();
}

}
