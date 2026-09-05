#pragma once

#include <hyprland/src/managers/input/trackpad/gestures/ITrackpadGesture.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>

class COverview;

enum class EExpoGestureAction {
    Expo,
    Cancel,
};

class CExpoGesture : public ITrackpadGesture {
  public:
    explicit CExpoGesture(EExpoGestureAction action) : m_action(action) {}
    virtual ~CExpoGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    COverview*    overview() const;

    // Monitor the gesture started on, so update/end keep driving the same
    // overview even when other monitors have one open too.
    PHLMONITORREF m_monitor;
    const EExpoGestureAction m_action;
    float                    m_lastDelta   = 0.F;
    bool                     m_firstUpdate = false;
};
