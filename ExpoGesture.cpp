#include "ExpoGesture.hpp"

#include "Overview.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/state/MonitorState.hpp>

void CExpoGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_lastDelta   = 0.F;
    m_firstUpdate = true;

    const auto monitor = State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();
    if (!monitor || !monitor->m_activeWorkspace)
        return;

    m_monitor = monitor;

    auto* const OV = overview();
    if (m_action == EExpoGestureAction::Cancel) {
        if (!OV || OV->closeCommitted())
            return;

        OV->beginCancelSwipe();
        return;
    }

    if (!OV)
        createOverview(monitor, true);
    else if (!OV->closeCommitted()) {
        OV->selectHoveredWorkspace();
        OV->setClosing(true);
    }
}

COverview* CExpoGesture::overview() const {
    return overviewForMonitor(m_monitor.lock());
}

void CExpoGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    auto* const OV = overview();
    if (!OV || OV->closeCommitted())
        return;

    if (m_firstUpdate) {
        m_firstUpdate = false;
        return;
    }

    m_lastDelta += distance(e);

    if (m_lastDelta <= 0.01) // plugin will crash if swipe ends at <= 0
        m_lastDelta = 0.01;

    OV->onSwipeUpdate(m_lastDelta);
}

void CExpoGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    auto* const OV = overview();
    if (!OV || OV->closeCommitted())
        return;

    OV->setClosing(false);
    OV->onSwipeEnd(m_action == EExpoGestureAction::Expo);
    // onSwipeEnd can tear the overview down, so re-resolve before touching it.
    if (auto* const STILL_ALIVE = overview())
        STILL_ALIVE->resetSwipe();
}
