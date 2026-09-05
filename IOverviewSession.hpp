#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>

#include "HyprexpoLogic.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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
    virtual bool selectHoveredWorkspace() = 0;
    virtual bool onKbMoveFocus(const std::string& direction) = 0;
    virtual bool onKbConfirm() = 0;
    virtual bool onKbSelectNumber(int number) = 0;
    virtual bool onKbSelectToken(int visibleIndex) = 0;
    virtual bool selectVisibleToken(const std::string& token) = 0;
    virtual int64_t selectedWorkspaceID() const = 0;
    virtual bool selectWorkspaceByID(int64_t workspaceID) = 0;
    virtual bool selectVisibleIndex(size_t index) = 0;
    virtual bool moveWindowBetweenVisibleIndices(size_t sourceIndex, size_t targetIndex, const PHLWINDOW& window = nullptr) = 0;

    virtual std::optional<Hyprexpo::SGlobalTile> focusedGlobalTile() const = 0;
    virtual std::vector<Hyprexpo::SGlobalTile> globalTiles() const = 0;
    virtual bool setKeyboardFocus(int tileIndex) = 0;
    virtual bool ownsAnimVar(const WP<Hyprutils::Animation::CBaseAnimatedVariable>& var) const = 0;
    virtual bool ownsPointerInput() const = 0;
    virtual bool ownsTouchInput(int32_t touchId) const { return false; }

    virtual bool blocksOverviewRendering() const = 0;
    virtual bool blocksDamageReporting() const = 0;
    virtual bool isSwiping() const = 0;
    virtual PHLMONITOR monitor() const = 0;
    virtual uint64_t sessionGeneration() const = 0;
};

// Registry ownership is removed before teardown callbacks can re-enter render/damage hooks.
inline std::vector<std::unique_ptr<IOverviewSession>> g_overviews;
inline PHLMONITORREF g_keyboardOverviewMonitor;

std::unique_ptr<IOverviewSession> createOverviewSession(const PHLWORKSPACE& startedOn, const PHLMONITOR& monitor, bool swipe = false);
IOverviewSession* activeOverview();
IOverviewSession* pointerOverview();
IOverviewSession* overviewForMonitor(const PHLMONITOR& monitor);
IOverviewSession* overviewForMonitorKey(uint64_t key);
IOverviewSession* overviewForSession(uint64_t monitorKey, uint64_t generation);
IOverviewSession* overviewForGlobalPoint(const Vector2D& point);
IOverviewSession* overviewForAnimVar(const WP<Hyprutils::Animation::CBaseAnimatedVariable>& var);
IOverviewSession* createOverview(const PHLMONITOR& monitor, bool swipe = false);
uint64_t overviewMonitorKey(const PHLMONITOR& monitor);
bool overviewRegistered(const IOverviewSession* overview);
bool overviewOpen();
std::vector<uint64_t> liveOverviewMonitorKeys();
bool moveOverviewFocusAcrossMonitors(IOverviewSession* source, Hyprexpo::EDirection direction);
void closeOverviewsSelecting(IOverviewSession* selecting);
void closeOverviews(bool switchToSelection);
void forEachOverview(const std::function<void(IOverviewSession&)>& fn);
void destroyOverview(IOverviewSession* overview);
void destroyAllOverviews();
void enterOverviewSubmap(bool& active);
void leaveOverviewSubmap(bool& active);
