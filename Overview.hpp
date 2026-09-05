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
#include <functional>
#include <memory>
#include <string>
#include <vector>

// saves on resources, but is a bit broken rn with blur.
// hyprland's fault, but cba to fix.
constexpr bool ENABLE_LOWRES = false;

class COverview final : public IOverviewSession {
  public:
    // The monitor is passed in rather than resolved from the cursor: with
    // several overviews alive they would all bind to the same output.
    COverview(PHLWORKSPACE startedOn_, PHLMONITOR monitor_, bool swipe = false, uint64_t sessionGeneration = 0);
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
    bool shouldRenderOverviewForMonitor(const PHLMONITOR& monitor) const;
    // Animation callbacks are handed the variable that fired, not the owner.
    // With several overviews alive the owner has to be resolved from it.
    bool ownsAnimVar(const WP<Hyprutils::Animation::CBaseAnimatedVariable>& var) const;
    void onWindowMoveToWorkspace(const PHLWINDOW& window, const PHLWORKSPACE& workspace);

    void resetSwipe() override;
    void onSwipeUpdate(double delta) override;
    void onSwipeEnd(bool switchToSelection) override;

    // close without a selection
    void          close(bool switchToSelection = true);
    bool          selectHoveredWorkspace();

    // keyboard navigation interface
    bool          onKbMoveFocus(const std::string& dir);
    bool          onKbConfirm();
    bool          onKbSelectNumber(int num);
    bool          onKbSelectToken(int visibleIdx);
    bool          selectVisibleToken(const std::string& token);
    int64_t       selectedWorkspaceID() const;
    bool          selectWorkspaceByID(int64_t workspaceID);
    bool          selectVisibleIndex(size_t index);
    bool          moveWindowBetweenVisibleIndices(size_t sourceIndex, size_t targetIndex, const PHLWINDOW& window = nullptr);
    std::optional<Hyprexpo::SGlobalTile> focusedGlobalTile() const;
    std::vector<Hyprexpo::SGlobalTile>   globalTiles() const;
    bool                                 setKeyboardFocus(int tileIndex);

    bool blocksOverviewRendering() const override { return blockOverviewRendering; }
    bool blocksDamageReporting() const override { return blockDamageReporting; }
    bool isSwiping() const override { return m_isSwiping; }
    bool ownsPointerInput() const override;
    PHLMONITOR monitor() const override { return pMonitor.lock(); }
    uint64_t sessionGeneration() const override { return m_sessionGeneration; }

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
    bool       moveFocus(int dx, int dy);
    int        tileForWorkspaceID(int wsid) const;
    int        tileForVisibleIndex(int vIdx) const;
    void       beginWindowDrag();
    bool       finishWindowDrag();
    void       updateWindowDrag();
    void       redrawDraggedWorkspace(int64_t workspaceID);
    void       queueRedrawID(int id);
    void       flushQueuedRedraws();
    PHLWINDOW  windowAtTilePoint(int id, const Vector2D& localPoint) const;
    Vector2D   tilePointToWorkspacePoint(int id, const Vector2D& localPoint) const;
    PHLWORKSPACE ensureWorkspaceForTile(int id);
    void       enterSubmapIfEnabled();
    void       resetSubmapIfNeeded();

    int        SIDE_LENGTH = 3;
    bool       dynamicGrid = false;
    bool       emptyTilesSelectable = false;
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

    std::vector<int>             queuedRedrawIDs;
    std::vector<int64_t>         settlingRedrawWorkspaceIDs;
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

struct SOverviewDragRuntime {
    Hyprexpo::SOverviewDragState state;
    Vector2D                     pressGlobal;
    Vector2D                     pointerGlobal;
    Vector2D                     grabOffset;
    PHLWINDOW                    window;
};

inline SOverviewDragRuntime g_overviewDrag;

// Grid drag indices are workspace tiles, while scrolling indices address its native scene.
COverview* gridOverviewForMonitorKey(uint64_t key);
COverview* gridOverviewForGlobalPoint(const Vector2D& point);
void resetOverviewDrag(Hyprexpo::EOverviewDragEventType type = Hyprexpo::EOverviewDragEventType::Cancel, uint64_t monitorKey = 0);
