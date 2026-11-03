#include "IOverviewSession.hpp"

#include "HyprlandConfigCompat.hpp"
#include "HyprexpoLogic.hpp"
#include "Overview.hpp"
#include "ScrollingLayoutAdapter.hpp"
#include "ScrollingOverview.hpp"
#include "globals.hpp"

#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include <atomic>
#include <exception>
#include <format>

namespace {

void notifyScrollingFailure(const std::string& message) {
    Log::logger->log(Log::ERR, Log::logFnName(), "[hyprexpo] {}", message);
    HyprlandAPI::addNotification(PHANDLE, "[hyprexpo] " + message, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
}

}

std::unique_ptr<IOverviewSession> createOverviewSession(const PHLWORKSPACE& startedOn, const PHLMONITOR& monitor, bool swipe, bool forceGrid) {
    if (!startedOn || !monitor || startedOn->m_monitor != monitor)
        return nullptr;
    static std::atomic<uint64_t> nextGeneration = 1;
    const uint64_t generation = nextGeneration.fetch_add(1, std::memory_order_relaxed);

    const std::string overviewMode = CompatHyprlandAPI::stringValue("plugin:hyprexpo:overview_mode");
    const auto modePreference = Hyprexpo::overviewModePreferenceFromString(overviewMode);
    // The caller's explicit grid request (expo next escalation) outranks the configured preference.
    const bool forcedGrid = forceGrid;
    const auto layout = Hyprexpo::Scrolling::workspaceUsesScrollingLayout(startedOn) ? Hyprexpo::EWorkspaceLayoutKind::Scrolling : Hyprexpo::EWorkspaceLayoutKind::Classic;
    const auto kind   = Hyprexpo::resolveOverviewSessionKind(modePreference, layout, forcedGrid);

    const bool detectedScrolling = Hyprexpo::sessionKindIsScrolling(kind);
    if (detectedScrolling) {
        const auto snapshot = Hyprexpo::Scrolling::snapshotWorkspace(startedOn);
        const bool emptyScrolling = startedOn && startedOn->getWindowCount() <= 0 && snapshot.failure == Hyprexpo::Scrolling::ESnapshotFailure::MissingScrollingData;
        if (!snapshot.success() && !emptyScrolling) {
            notifyScrollingFailure(std::format("native scrolling snapshot failed ({}): {}", snapshotFailureName(snapshot.failure), snapshot.error));
            return nullptr;
        }
        try {
            auto scrolling = std::make_unique<CScrollingOverview>(startedOn, monitor, swipe, generation, snapshot.snapshot, kind);
            if (scrolling->valid())
                return scrolling;
            notifyScrollingFailure("native scrolling session initialization failed");
        } catch (const std::exception& error) {
            notifyScrollingFailure(std::format("native scrolling session threw during initialization: {}", error.what()));
        } catch (...) {
            notifyScrollingFailure("native scrolling session threw an unknown exception");
        }
        return nullptr;
    }

    return std::make_unique<COverview>(startedOn, monitor, swipe, generation);
}
