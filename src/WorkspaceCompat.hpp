#pragma once

#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/state/workspace/Resolver.hpp>
#include <hyprland/src/workspace/HLWorkspace.hpp>

#include <cstdint>
#include <limits>
#include <string>

using WORKSPACEID = int64_t;

constexpr WORKSPACEID WORKSPACE_INVALID = -1;

inline WORKSPACEID workspaceID(const PHLWORKSPACE& workspace) {
    if (!workspace)
        return WORKSPACE_INVALID;

    const auto numbered = workspace->numberedID();
    return numbered ? static_cast<WORKSPACEID>(*numbered) : WORKSPACE_INVALID;
}

inline WORKSPACEID activeWorkspaceID(const PHLMONITOR& monitor) {
    return monitor ? workspaceID(monitor->m_activeWorkspace) : WORKSPACE_INVALID;
}

inline bool workspaceHasID(const PHLWORKSPACE& workspace, WORKSPACEID id) {
    return workspaceID(workspace) == id;
}

inline PHLWORKSPACE findWorkspaceByID(WORKSPACEID id) {
    if (id <= 0 || id > static_cast<WORKSPACEID>(std::numeric_limits<Workspace::WorkspaceIDContainer>::max()))
        return nullptr;

    return State::workspaceState()->query().numbered(Workspace::SWorkspaceNumberedID{static_cast<Workspace::WorkspaceIDContainer>(id)}).run();
}

inline PHLWORKSPACE createWorkspaceForMonitor(WORKSPACEID id, PHLMONITOR monitor, bool isEmpty = false) {
    if (id <= 0 || id > static_cast<WORKSPACEID>(std::numeric_limits<Workspace::WorkspaceIDContainer>::max()) || !monitor)
        return nullptr;

    return State::workspaceState()->createNumbered(Workspace::SWorkspaceNumberedID{static_cast<Workspace::WorkspaceIDContainer>(id)}, std::move(monitor), std::to_string(id), isEmpty);
}

inline WORKSPACEID workspaceIDForSelector(const PHLMONITOR& monitor, const std::string& selector) {
    if (!monitor)
        return WORKSPACE_INVALID;

    const auto target = State::Workspace::resolver()->getWorkspaceTargetFromString(selector, monitor);
    if (!target.id)
        return WORKSPACE_INVALID;

    const auto numbered = std::get_if<Workspace::SWorkspaceNumberedID>(&*target.id);
    return numbered ? static_cast<WORKSPACEID>(numbered->value) : WORKSPACE_INVALID;
}
