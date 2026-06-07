#include "ExpoGesture.hpp"

#include "Overview.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/helpers/Monitor.hpp>

void CExpoGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_lastDelta   = 0.F;
    m_firstUpdate = true;

    const auto monitor = g_pCompositor->getMonitorFromCursor();
    if (!monitor || !monitor->m_activeWorkspace)
        return;

    if (!g_pOverview)
        g_pOverview = std::make_unique<COverview>(monitor->m_activeWorkspace, true);
    else {
        g_pOverview->selectHoveredWorkspace();
        g_pOverview->setClosing(true);
    }
}

void CExpoGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!g_pOverview)
        return;

    if (m_firstUpdate) {
        m_firstUpdate = false;
        return;
    }

    m_lastDelta += distance(e);

    if (m_lastDelta <= 0.01) // plugin will crash if swipe ends at <= 0
        m_lastDelta = 0.01;

    g_pOverview->onSwipeUpdate(m_lastDelta);
}

void CExpoGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (!g_pOverview)
        return;

    g_pOverview->setClosing(false);
    g_pOverview->onSwipeEnd();
    if (g_pOverview)
        g_pOverview->resetSwipe();
}
