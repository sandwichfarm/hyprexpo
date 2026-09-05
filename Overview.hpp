#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
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

class COverview {
  public:
    // The monitor is passed in rather than resolved from the cursor: with
    // several overviews alive they would all bind to the same output.
    COverview(PHLWORKSPACE startedOn_, PHLMONITOR monitor_, bool swipe = false);
    ~COverview();

    void render();
    void damage();
    void onDamageReported();
    void onPreRender();

    void setClosing(bool closing);
    void beginCancelSwipe();
    // True once close() has armed the teardown animation. Further gestures must
    // be ignored until the overview is destroyed, otherwise a second swipe
    // rewinds the in-flight close animation (the close "replays" from ~80%).
    bool closeCommitted() const {
        return m_closeCommitted;
    }
    bool shouldRenderOverviewForMonitor(const PHLMONITOR& monitor) const;
    // Animation callbacks are handed the variable that fired, not the owner.
    // With several overviews alive the owner has to be resolved from it.
    bool ownsAnimVar(const WP<Hyprutils::Animation::CBaseAnimatedVariable>& var) const;
    void onWindowMoveToWorkspace(const PHLWINDOW& window, const PHLWORKSPACE& workspace);

    void resetSwipe();
    void onSwipeUpdate(double delta);
    void onSwipeEnd(bool switchToSelection);

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
    void       fullRender();
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

// Overview instances, one per monitor currently showing the overview. A single
// monitor invocation keeps exactly one entry; "all monitors" mode keeps one per
// output. Instances are owned here and torn down via destroyOverview().
inline std::vector<std::unique_ptr<COverview>> g_overviews;
inline PHLMONITORREF                           g_keyboardOverviewMonitor;

struct SOverviewDragRuntime {
    Hyprexpo::SOverviewDragState state;
    Vector2D                     pressGlobal;
    Vector2D                     pointerGlobal;
    Vector2D                     grabOffset;
    PHLWINDOW                    window;
};

inline SOverviewDragRuntime g_overviewDrag;

// The instance that owns keyboard input and unqualified dispatcher commands:
// the one on the focused monitor, else the one under the cursor, else the
// first. nullptr when no overview is open.
COverview* activeOverview();
COverview* overviewForMonitorKey(uint64_t key);
COverview* overviewForGlobalPoint(const Vector2D& point);
uint64_t   overviewMonitorKey(const PHLMONITOR& monitor);
bool       overviewRegistered(const COverview* overview);
std::vector<uint64_t> liveOverviewMonitorKeys();
bool       moveOverviewFocusAcrossMonitors(COverview* source, Hyprexpo::EDirection direction);
void       closeOverviewsSelecting(COverview* selecting);
void       closeOverviews(bool switchToSelection);
void       resetOverviewDrag(Hyprexpo::EOverviewDragEventType type = Hyprexpo::EOverviewDragEventType::Cancel, uint64_t monitorKey = 0);

// The instance owning an animated variable, or nullptr.
COverview* overviewForAnimVar(const WP<Hyprutils::Animation::CBaseAnimatedVariable>& var);

// The instance rendering on a specific monitor, or nullptr.
COverview* overviewForMonitor(const PHLMONITOR& monitor);

// Create an overview on a monitor and register it. Returns nullptr if the
// monitor already has one, or if the arguments are unusable.
COverview* createOverview(const PHLMONITOR& monitor, bool swipe = false);

// True while at least one overview is alive.
bool       overviewOpen();

// Run fn for every live overview. Safe when the callback destroys overviews:
// entries torn down mid-iteration are skipped.
void       forEachOverview(const std::function<void(COverview&)>& fn);

// Destroy one instance. This deletes the object, so a member function
// destroying itself must return immediately afterwards.
void       destroyOverview(COverview* overview);

// Destroy every instance.
void       destroyAllOverviews();
