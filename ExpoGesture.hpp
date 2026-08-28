#pragma once

#include <hyprland/src/managers/input/trackpad/gestures/ITrackpadGesture.hpp>

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
    const EExpoGestureAction m_action;
    float                    m_lastDelta   = 0.F;
    bool                     m_firstUpdate = false;
};
