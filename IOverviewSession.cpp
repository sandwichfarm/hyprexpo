#include "IOverviewSession.hpp"

#include "Overview.hpp"
#include "ScrollingLayoutAdapter.hpp"
#include "ScrollingOverview.hpp"

#include <atomic>

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
        } catch (...) {}
    }

    return std::make_unique<COverview>(startedOn, swipe, generation);
}
