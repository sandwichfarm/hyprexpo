#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

class IOverviewSession {
  public:
    virtual ~IOverviewSession() = default;

    virtual void render() = 0;
    virtual void damage() = 0;
    virtual void onDamageReported() = 0;
    virtual void onPreRender() = 0;
    virtual void onConfigReload() = 0;
    virtual void prepareForTeardown() = 0;
    virtual std::expected<std::string, std::string> injectScrollingInput(const std::string& sequence) = 0;
    virtual void fullRender() = 0;

    virtual void setClosing(bool closing) = 0;
    virtual void beginCancelSwipe() = 0;
    virtual bool closeCommitted() const = 0;
    virtual bool shouldRenderOverviewForMonitor(const PHLMONITOR& monitor) const = 0;
    virtual void onWindowMoveToWorkspace(const PHLWINDOW& window, const PHLWORKSPACE& workspace) = 0;

    virtual void resetSwipe() = 0;
    virtual void onSwipeUpdate(double delta) = 0;
    virtual void onSwipeEnd(bool switchToSelection) = 0;

    virtual void close(bool switchToSelection = true) = 0;
    virtual void selectHoveredWorkspace() = 0;
    virtual void onKbMoveFocus(const std::string& direction) = 0;
    virtual void onKbConfirm() = 0;
    virtual void onKbSelectNumber(int number) = 0;
    virtual void onKbSelectToken(int visibleIndex) = 0;
    virtual bool selectVisibleToken(const std::string& token) = 0;
    virtual int64_t selectedWorkspaceID() const = 0;
    virtual bool selectWorkspaceByID(int64_t workspaceID) = 0;
    virtual bool selectVisibleIndex(size_t index) = 0;
    virtual bool moveWindowBetweenVisibleIndices(size_t sourceIndex, size_t targetIndex, const PHLWINDOW& window = nullptr) = 0;

    virtual bool blocksOverviewRendering() const = 0;
    virtual bool blocksDamageReporting() const = 0;
    virtual bool isSwiping() const = 0;
    virtual PHLMONITOR monitor() const = 0;
    virtual uint64_t sessionGeneration() const = 0;
};

inline std::unique_ptr<IOverviewSession> g_pOverview;

std::unique_ptr<IOverviewSession> createOverviewSession(const PHLWORKSPACE& startedOn, bool swipe = false);
