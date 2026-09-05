#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include "IOverviewSession.hpp"
#include "HyprexpoLogic.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopTimer.hpp>
#include <chrono>
#include <string>
#include <vector>

// saves on resources, but is a bit broken rn with blur.
// hyprland's fault, but cba to fix.
constexpr bool ENABLE_LOWRES = false;

class COverview final : public IOverviewSession {
  public:
    COverview(PHLWORKSPACE startedOn_, bool swipe = false, uint64_t sessionGeneration = 0);
    ~COverview() override;

    void render() override;
    void damage() override;
    void onDamageReported() override;
    void onPreRender() override;
    void onConfigReload() override {
        onDamageReported();
    }
    void prepareForTeardown() override {}
    std::expected<std::string, std::string> injectScrollingInput(const std::string&) override {
        return std::unexpected("active overview is not a scrolling session");
    }

    void setClosing(bool closing) override;
    void beginCancelSwipe() override;
    // True once close() has armed the teardown animation. Further gestures must
    // be ignored until the overview is destroyed, otherwise a second swipe
    // rewinds the in-flight close animation (the close "replays" from ~80%).
    bool closeCommitted() const override {
        return m_closeCommitted;
    }
    bool shouldRenderOverviewForMonitor(const PHLMONITOR& monitor) const override;
    void onWindowMoveToWorkspace(const PHLWINDOW& window, const PHLWORKSPACE& workspace) override;

    void resetSwipe() override;
    void onSwipeUpdate(double delta) override;
    void onSwipeEnd(bool switchToSelection) override;

    // close without a selection
    void          close(bool switchToSelection = true) override;
    void          selectHoveredWorkspace() override;

    // keyboard navigation interface
    void          onKbMoveFocus(const std::string& dir) override;
    void          onKbConfirm() override;
    void          onKbSelectNumber(int num) override;
    void          onKbSelectToken(int visibleIdx) override;
    bool          selectVisibleToken(const std::string& token) override;
    int64_t       selectedWorkspaceID() const override;
    bool          selectWorkspaceByID(int64_t workspaceID) override;
    bool          selectVisibleIndex(size_t index) override;
    bool          moveWindowBetweenVisibleIndices(size_t sourceIndex, size_t targetIndex, const PHLWINDOW& window = nullptr) override;

    bool blocksOverviewRendering() const override {
        return blockOverviewRendering;
    }
    bool blocksDamageReporting() const override {
        return blockDamageReporting;
    }
    bool isSwiping() const override {
        return m_isSwiping;
    }
    PHLMONITOR monitor() const override {
        return pMonitor.lock();
    }
    uint64_t sessionGeneration() const override {
        return m_sessionGeneration;
    }

    bool          blockOverviewRendering = false;
    bool          blockDamageReporting   = false;

    PHLMONITORREF pMonitor;
    bool          m_isSwiping = false;

    struct SWorkspaceImage {
        SP<Render::IFramebuffer> fb;
        int64_t                  workspaceID = -1;
        PHLWORKSPACE             pWorkspace;
        CBox                     box;
        // Label textures per state for customization
        SP<Render::ITexture>     labelTexDefault;
        SP<Render::ITexture>     labelTexHover;
        SP<Render::ITexture>     labelTexFocus;
        SP<Render::ITexture>     labelTexCurrent;
        SP<Render::ITexture>     selectionLabelTex;
        Vector2D                 labelSizeDefault = {0, 0};
        Vector2D                 labelSizeHover   = {0, 0};
        Vector2D                 labelSizeFocus   = {0, 0};
        Vector2D                 labelSizeCurrent = {0, 0};
        Vector2D                 selectionLabelSize = {0, 0};
    };

  private:
    void       redrawID(int id, bool forcelowres = false);
    void       redrawAll(bool forcelowres = false);
    void       onWorkspaceChange();
    void       fullRender() override;
    Hyprexpo::SGridShape currentGridShape() const;
    double     currentOuterInset() const;
    Hyprexpo::STileLayout tileLayoutForIndex(int id, const Vector2D& totalSize, double gap, double outerInset = 0.0, bool centerPartialRows = true) const;
    CBox       tileBoxForIndex(int id, const Vector2D& totalSize, double gap, double outerInset = 0.0, bool centerPartialRows = true) const;
    int        tileIndexAtPoint(const Vector2D& point, const Vector2D& totalSize, double gap, double outerInset = 0.0, bool centerPartialRows = true) const;
    Vector2D   tilePosForID(int id, const Vector2D& totalSize, double gap, double outerInset = 0.0, bool centerPartialRows = true) const;
    Vector2D   zoomSizeForCurrentGrid(const Vector2D& monitorSize) const;
    void       updateHoveredFromMouse();
    void       ensureKbFocusInitialized();
    bool       isTileValid(int id) const;
    void       moveFocus(int dx, int dy);
    int        tileForWorkspaceID(int wsid) const;
    int        tileForVisibleIndex(int vIdx) const;
    void       beginWindowDrag();
    bool       finishWindowDrag();
    void       updateWindowDrag();
    void       redrawDraggedWindowTiles(int source, int target);
    void       queueRedrawID(int id);
    void       flushQueuedRedraws();
    PHLWINDOW  windowAtTilePoint(int id, const Vector2D& localPoint) const;
    Vector2D   tilePointToWorkspacePoint(int id, const Vector2D& localPoint) const;
    PHLWORKSPACE ensureWorkspaceForTile(int id);
    void       enterSubmapIfEnabled();
    void       resetSubmapIfNeeded();

    int        SIDE_LENGTH = 3;
    bool       dynamicGrid = false;
    Hyprexpo::SGridShape gridShape{3, 3};
    int        GAP_WIDTH   = 5;
    CHyprColor BG_COLOR    = CHyprColor{0.1, 0.1, 0.1, 1.0};

    bool       damageDirty = false;

    Vector2D                     lastMousePosLocal = Vector2D{};

    int                          openedID  = -1;
    int                          closeOnID = -1;
    int                          kbFocusID = -1;
    int                          hoveredID = -1;
    bool                         submapActive = false;
    std::string                  previousSubmap = "";

    Vector2D                     dragStartLocal = Vector2D{};
    int                          dragSourceID   = -1;
    bool                         dragMoved      = false;
    Vector2D                     dragGrabOffset = Vector2D{};
    PHLWINDOW                    dragWindow;
    int                          dropIntentTargetID = -1;
    Hyprexpo::SDropIntentGeometry dropIntent;

    std::vector<int>             queuedRedrawIDs;
    std::vector<int>             settlingRedrawIDs;
    int                          redrawSettleTicks = 0;
    SP<CEventLoopTimer>          redrawSettleTimer;

    std::vector<SWorkspaceImage> images;

    PHLWORKSPACE                 startedOn;

    PHLANIMVAR<Vector2D>         size;
    PHLANIMVAR<Vector2D>         pos;

    bool                         closing = false;
    bool                         m_closeCommitted = false;
    uint64_t                     m_sessionGeneration = 0;
    bool                         externalWorkspaceMoveDuringClose = false;

    CHyprSignalListener          mouseMoveHook;
    CHyprSignalListener          mouseButtonHook;
    CHyprSignalListener          touchMoveHook;
    CHyprSignalListener          touchDownHook;
    CHyprSignalListener          workspaceMoveHook;

    bool                         swipe             = false;
    bool                         swipeWasCommenced = false;
    bool                         showWorkspaceNumbers = false;
    bool                         showWorkspaceNames = false;
    bool                         animateEntry = false;
    bool                         wallpaperBg = false;
    std::chrono::steady_clock::time_point createdAt;

    friend class COverviewPassElement;
};
