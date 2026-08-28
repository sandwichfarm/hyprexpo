#include "IOverviewSession.hpp"

#include "Overview.hpp"
#include "ScrollingLayoutAdapter.hpp"
#include "ScrollingOverview.hpp"

#include <hyprland/src/debug/log/Logger.hpp>

#include <atomic>
#include <exception>

std::unique_ptr<IOverviewSession> createOverviewSession(const PHLWORKSPACE& startedOn, bool swipe) {
    static std::atomic<uint64_t> nextGeneration = 1;
    const uint64_t generation = nextGeneration.fetch_add(1, std::memory_order_relaxed);

    const bool detectedScrolling = Hyprexpo::Scrolling::workspaceUsesScrollingLayout(startedOn);
    const auto snapshot = Hyprexpo::Scrolling::snapshotWorkspace(startedOn);
    const bool emptyScrolling = detectedScrolling && startedOn && startedOn->getWindowCount() <= 0 && snapshot.failure == Hyprexpo::Scrolling::ESnapshotFailure::MissingScrollingData;
    if (detectedScrolling && (snapshot.success() || emptyScrolling)) {
        try {
            auto scrolling = std::make_unique<CScrollingOverview>(startedOn, swipe, generation, snapshot.snapshot);
            if (scrolling->valid())
                return scrolling;
            Log::logger->log(Log::ERR, "[hyprexpo] native scrolling session initialization failed; using grid fallback");
        } catch (const std::exception& error) {
            Log::logger->log(Log::ERR, "[hyprexpo] native scrolling session threw during initialization: {}; using grid fallback", error.what());
        } catch (...) {
            Log::logger->log(Log::ERR, "[hyprexpo] native scrolling session threw an unknown exception; using grid fallback");
        }
    } else if (detectedScrolling) {
        Log::logger->log(Log::ERR, "[hyprexpo] native scrolling snapshot failed ({}): {}; using grid fallback", snapshotFailureName(snapshot.failure), snapshot.error);
    }

    return std::make_unique<COverview>(startedOn, swipe, generation);
}
