#include "HyprlandConfigCompat.hpp"
#define HyprlandAPI CompatHyprlandAPI
#include "OverviewInternal.hpp"
#include "HyprexpoLogic.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/state/GlobalWindowController.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/pointer/cursor/CursorShapeOverrideController.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

void COverview::selectHoveredWorkspace() {
    if (closing)
        return;

    updateHoveredFromMouse();
    closeOnID = std::clamp(hoveredID, 0, SIDE_LENGTH * SIDE_LENGTH - 1);
}

int64_t COverview::selectedWorkspaceID() const {
    const int id = closeOnID == -1 ? openedID : closeOnID;
    if (id < 0 || id >= (int)images.size())
        return WORKSPACE_INVALID;

    return images[id].workspaceID;
}

bool COverview::selectWorkspaceByID(int64_t workspaceID) {
    if (closing)
        return false;

    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i].workspaceID != workspaceID)
            continue;

        closeOnID = i;
        return true;
    }

    return false;
}

bool COverview::selectVisibleIndex(size_t index) {
    if (closing)
        return false;

    size_t visible = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i].workspaceID == WORKSPACE_INVALID)
            continue;

        if (visible == index) {
            closeOnID = i;
            return true;
        }

        ++visible;
    }

    return false;
}

void COverview::updateHoveredFromMouse() {
    const auto MON = pMonitor.lock();
    if (!MON)
        return;

    const int newHoveredID = Hyprexpo::tileIndexFromPoint(lastMousePosLocal.x, lastMousePosLocal.y, MON->m_size.x, MON->m_size.y, SIDE_LENGTH);
    if (newHoveredID == hoveredID)
        return;

    hoveredID = newHoveredID;
    damage();
}

void COverview::ensureKbFocusInitialized() {
    if (kbFocusID != -1)
        return;

    // try to set to current openedID
    if (openedID != -1) {
        kbFocusID = openedID;
        return;
    }

    // fallback: first valid tile
    for (size_t i = 0; i < images.size(); ++i) {
        if (isTileValid(i)) {
            kbFocusID = i;
            return;
        }
    }
}

bool COverview::isTileValid(int id) const {
    if (id < 0 || id >= (int)images.size())
        return false;
    return images[id].workspaceID != WORKSPACE_INVALID;
}

int COverview::tileForWorkspaceID(int wsid) const {
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i].workspaceID == wsid)
            return (int)i;
    }
    return -1;
}

int COverview::tileForVisibleIndex(int vIdx) const {
    if (vIdx < 0)
        return -1;
    int seen = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i].workspaceID == WORKSPACE_INVALID)
            continue;
        if (seen == vIdx)
            return (int)i;
        ++seen;
    }
    return -1;
}

Vector2D COverview::tilePointToWorkspacePoint(int id, const Vector2D& localPoint) const {
    const auto MON = pMonitor.lock();
    if (!MON)
        return {};

    const Vector2D tileSize = MON->m_size / SIDE_LENGTH;
    const Vector2D tilePos  = tileSize * Vector2D{id % SIDE_LENGTH, id / SIDE_LENGTH};
    const Vector2D inTile   = localPoint - tilePos;

    return MON->m_position + Vector2D{
        std::clamp(inTile.x / tileSize.x, 0.0, 1.0) * MON->m_size.x,
        std::clamp(inTile.y / tileSize.y, 0.0, 1.0) * MON->m_size.y,
    };
}

PHLWINDOW COverview::windowAtTilePoint(int id, const Vector2D& localPoint) const {
    if (!isTileValid(id))
        return nullptr;

    PHLWORKSPACE WORKSPACE;
    if (images[id].pWorkspace) {
        WORKSPACE = images[id].pWorkspace;
    }
    else {
        for (const auto& w : State::workspaceState()->workspacesCopy()) {
            if (w->m_id == images[id].workspaceID) {
                WORKSPACE = w;
                break;
            }
        }
    }

    if (!WORKSPACE)
        return nullptr;

    const auto POINT = tilePointToWorkspacePoint(id, localPoint);
    const auto& windows = Desktop::windowState()->windows();
    for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
        const auto& window = *it;
        if (!windowVisibleOnWorkspace(window, WORKSPACE))
            continue;

        if (window->getWindowMainSurfaceBox().containsPoint(POINT))
            return window;
    }

    return nullptr;
}

void COverview::beginWindowDrag() {
    updateHoveredFromMouse();
    dragStartLocal = lastMousePosLocal;
    dragSourceID   = hoveredID;
    dragMoved      = false;
    dragWindow     = windowAtTilePoint(dragSourceID, dragStartLocal);
    dragGrabOffset = Vector2D{};
    dropIntent         = {};
    dropIntentTargetID = -1;

    if (!dragWindow)
        return;

    const auto POINT = tilePointToWorkspacePoint(dragSourceID, dragStartLocal);
    const auto BOX   = dragWindow->getWindowMainSurfaceBox();
    dragGrabOffset   = POINT - Vector2D{BOX.x, BOX.y};
    Pointer::Cursor::overrideController->setOverride("grabbing", Pointer::Cursor::CURSOR_OVERRIDE_UNKNOWN);
    damage();
}

void COverview::updateWindowDrag() {
    if (!dragWindow)
        return;

    const auto dx = lastMousePosLocal.x - dragStartLocal.x;
    const auto dy = lastMousePosLocal.y - dragStartLocal.y;
    if (!dragMoved && std::hypot(dx, dy) < 12.0)
        return;

    if (!dragMoved)
        dragMoved = true;
    damage();
}

PHLWORKSPACE COverview::ensureWorkspaceForTile(int id) {
    if (!isTileValid(id))
        return nullptr;

    const auto MON = pMonitor.lock();
    if (!MON)
        return nullptr;

    auto& image = images[id];
    if (image.pWorkspace)
        return image.pWorkspace;

    PHLWORKSPACE workspace;
    for (const auto& w : State::workspaceState()->workspacesCopy()) {
        if (w->m_id == image.workspaceID) {
            workspace = w;
            break;
        }
    }

    if (!workspace)
        workspace = State::workspaceState()->create(image.workspaceID, MON->m_id, std::to_string(image.workspaceID), false);

    image.pWorkspace = workspace;
    return workspace;
}

bool COverview::finishWindowDrag() {
    const auto WINDOW = dragWindow;
    const int  SOURCE = dragSourceID;
    const bool MOVED  = dragMoved;

    dragWindow     = nullptr;
    dragSourceID   = -1;
    dragMoved      = false;
    dragGrabOffset = Vector2D{};
    dropIntent         = {};
    dropIntentTargetID = -1;
    Pointer::Cursor::overrideController->setOverride("left_ptr", Pointer::Cursor::CURSOR_OVERRIDE_UNKNOWN);

    if (!WINDOW || !MOVED)
        return false;

    // Drag proxies are render-only; repaint even when release does not move workspace contents.
    damage();
    updateHoveredFromMouse();

    const int TARGET = hoveredID;
    if (!isTileValid(SOURCE) || !isTileValid(TARGET) || SOURCE == TARGET)
        return true;

    PHLWORKSPACE SOURCEWS;
    if (images[SOURCE].pWorkspace) {
        SOURCEWS = images[SOURCE].pWorkspace;
    }
    else {
        for (const auto& w : State::workspaceState()->workspacesCopy()) {
            if (w->m_id == images[SOURCE].workspaceID) {
                SOURCEWS = w;
                break;
            }
        }
    }

    const auto TARGETWS = ensureWorkspaceForTile(TARGET);

    if (!windowVisibleOnWorkspace(WINDOW, SOURCEWS) || !TARGETWS || TARGETWS == SOURCEWS)
        return true;

    images[SOURCE].pWorkspace = SOURCEWS;
    Desktop::globalWindowController()->moveWindowToWorkspace(WINDOW, TARGETWS);
    settleWorkspaceMoveAnimation(WINDOW);
    redrawDraggedWindowTiles(SOURCE, TARGET);
    return true;
}

bool COverview::moveWindowBetweenVisibleIndices(size_t sourceIndex, size_t targetIndex, const PHLWINDOW& requestedWindow) {
    if (closing)
        return false;

    const int SOURCE = tileForVisibleIndex(sourceIndex);
    const int TARGET = tileForVisibleIndex(targetIndex);
    if (!isTileValid(SOURCE) || !isTileValid(TARGET) || SOURCE == TARGET)
        return false;

    PHLWORKSPACE SOURCEWS;
    if (images[SOURCE].pWorkspace) {
        SOURCEWS = images[SOURCE].pWorkspace;
    }
    else {
        for (const auto& w : State::workspaceState()->workspacesCopy()) {
            if (w->m_id == images[SOURCE].workspaceID) {
                SOURCEWS = w;
                break;
            }
        }
    }

    const auto TARGETWS = ensureWorkspaceForTile(TARGET);

    if (!SOURCEWS || !TARGETWS || SOURCEWS == TARGETWS)
        return false;

    PHLWINDOW window = requestedWindow;
    if (window) {
        if (!windowVisibleOnWorkspace(window, SOURCEWS))
            return false;
    } else {
        const auto& windows = Desktop::windowState()->windows();
        for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
            const auto& candidate = *it;
            if (!windowVisibleOnWorkspace(candidate, SOURCEWS))
                continue;

            window = candidate;
            break;
        }
    }

    if (!window)
        return false;

    images[SOURCE].pWorkspace = SOURCEWS;
    Desktop::globalWindowController()->moveWindowToWorkspace(window, TARGETWS);
    settleWorkspaceMoveAnimation(window);
    redrawDraggedWindowTiles(SOURCE, TARGET);
    return true;
}

void COverview::redrawDraggedWindowTiles(int source, int target) {
    queueRedrawID(source);
    queueRedrawID(target);

    for (const auto id : queuedRedrawIDs) {
        if (std::find(settlingRedrawIDs.begin(), settlingRedrawIDs.end(), id) == settlingRedrawIDs.end())
            settlingRedrawIDs.push_back(id);
    }
    redrawSettleTicks = 4;

    flushQueuedRedraws();

    if (redrawSettleTimer)
        return;

    redrawSettleTimer = makeShared<CEventLoopTimer>(
        75ms,
        [this](SP<CEventLoopTimer> self, void*) {
            if (!g_pOverview || g_pOverview.get() != this || closing) {
                self->cancel();
                redrawSettleTimer.reset();
                return;
            }

            for (const auto id : settlingRedrawIDs)
                queueRedrawID(id);

            flushQueuedRedraws();

            if (--redrawSettleTicks <= 0) {
                settlingRedrawIDs.clear();
                redrawSettleTimer.reset();
                self->cancel();
                return;
            }

            self->updateTimeout(100ms);
        },
        nullptr);
    g_pEventLoopManager->addTimer(redrawSettleTimer);
}

void COverview::queueRedrawID(int id) {
    if (!isTileValid(id))
        return;

    if (std::find(queuedRedrawIDs.begin(), queuedRedrawIDs.end(), id) == queuedRedrawIDs.end())
        queuedRedrawIDs.push_back(id);
}

void COverview::flushQueuedRedraws() {
    if (queuedRedrawIDs.empty())
        return;

    const auto ids = queuedRedrawIDs;
    queuedRedrawIDs.clear();

    for (const auto id : ids)
        redrawID(id);

    damage();
}

bool COverview::selectVisibleToken(const std::string& token) {
    if (closing)
        return false;

    static auto* const* PSELECTLABEL = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:selection_label_enable")->getDataStaticPtr();
    static auto const*  PSELECTMAP   = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:selection_label_token_map")->getDataStaticPtr();

    const std::string normalized = lowerString(trimString(token));
    if (normalized.empty())
        return false;

    if (**PSELECTLABEL) {
        const auto tokens = splitCommaList(std::string{*PSELECTMAP});
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i].empty() || lowerString(tokens[i]) != normalized)
                continue;

            return selectVisibleIndex(i);
        }

        return false;
    }

    const int visibleIndex = fallbackTokenToVisibleIndex(normalized);
    if (visibleIndex < 0)
        return false;

    return selectVisibleIndex(visibleIndex);
}

void COverview::moveFocus(int dx, int dy) {
    ensureKbFocusInitialized();
    if (kbFocusID == -1)
        return;

    int x = kbFocusID % SIDE_LENGTH;
    int y = kbFocusID / SIDE_LENGTH;

    static auto* const* PWRAPH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:keynav_wrap_h")->getDataStaticPtr();
    static auto* const* PWRAPV = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:keynav_wrap_v")->getDataStaticPtr();

    if (dx != 0) {
        static auto* const* PREADING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:keynav_reading_order")->getDataStaticPtr();
        int                 step     = dx > 0 ? 1 : -1;
        if (**PREADING) {
            // reading-order scan: proceed linearly across the grid (row-major)
            const int total = SIDE_LENGTH * SIDE_LENGTH;
            int       idx   = kbFocusID;
            for (int tries = 0; tries < total; ++tries) {
                idx += step;
                if (idx < 0 || idx >= total) {
                    // wrap only if both wraps are enabled (edge of grid)
                    if (**PWRAPH && **PWRAPV)
                        idx = (idx + total) % total;
                    else
                        break;
                }
                if (isTileValid(idx)) {
                    kbFocusID = idx;
                    return;
                }
            }
        } else {
            // in-row scan with optional horizontal wrap
            int nx = x;
            for (int tries = 0; tries < SIDE_LENGTH; ++tries) {
                nx += step;
                if (nx < 0 || nx >= SIDE_LENGTH) {
                    if (**PWRAPH)
                        nx = (nx + SIDE_LENGTH) % SIDE_LENGTH;
                    else
                        break;
                }
                const int nid = nx + y * SIDE_LENGTH;
                if (isTileValid(nid)) {
                    kbFocusID = nid;
                    return;
                }
            }
        }
    }

    if (dy != 0) {
        int step = dy > 0 ? 1 : -1;
        int ny   = y;
        for (int tries = 0; tries < SIDE_LENGTH; ++tries) {
            ny += step;
            if (ny < 0 || ny >= SIDE_LENGTH) {
                if (**PWRAPV)
                    ny = (ny + SIDE_LENGTH) % SIDE_LENGTH;
                else
                    break;
            }
            const int nid = x + ny * SIDE_LENGTH;
            if (isTileValid(nid)) {
                kbFocusID = nid;
                return;
            }
        }
    }
}

void COverview::onKbMoveFocus(const std::string& dir) {
    if (closing)
        return;
    if (dir == "left")
        moveFocus(-1, 0);
    else if (dir == "right")
        moveFocus(1, 0);
    else if (dir == "up")
        moveFocus(0, -1);
    else if (dir == "down")
        moveFocus(0, 1);

    damage();
}

void COverview::onKbConfirm() {
    if (closing)
        return;
    ensureKbFocusInitialized();
    if (kbFocusID != -1)
        closeOnID = kbFocusID;
    close();
}

void COverview::onKbSelectNumber(int num) {
    if (closing)
        return;

    if (num == 0)
        num = 10;

    if (selectWorkspaceByID(num))
        close();
}

void COverview::onKbSelectToken(int visibleIdx) {
    if (closing)
        return;
    if (visibleIdx < 0)
        return;
    if (selectVisibleIndex(visibleIdx))
        close();
}

static float lerpFloat(const float& from, const float& to, const float perc) {
    return (to - from) * perc + from;
}

static Vector2D lerp(const Vector2D& from, const Vector2D& to, const float perc) {
    return Vector2D{lerpFloat(from.x, to.x, perc), lerpFloat(from.y, to.y, perc)};
}

void COverview::setClosing(bool closing_) {
    closing = closing_;
}

void COverview::onWindowMoveToWorkspace(const PHLWINDOW& window, const PHLWORKSPACE& workspace) {
    if (!closing || externalWorkspaceMoveDuringClose || !window)
        return;

    const auto monitor = pMonitor.lock();
    if (!monitor)
        return;

    const bool movedOnOverviewMonitor = window->m_monitor == monitor || (window->m_workspace && window->m_workspace->m_monitor == monitor) || (workspace && workspace->m_monitor == monitor);
    if (!movedOnOverviewMonitor)
        return;

    externalWorkspaceMoveDuringClose = true;
    damage();
    pMonitor->scheduleFrame();
}

void COverview::resetSwipe() {
    swipeWasCommenced = false;
}

void COverview::onSwipeUpdate(double delta) {
    // Once the close animation is committed, ignore further swipe input so a
    // re-grabbed gesture can't warp size/pos back and replay the close.
    if (m_closeCommitted)
        return;

    m_isSwiping = true;

    const auto MON = pMonitor.lock();
    if (!MON) {
        m_isSwiping = false;
        return;
    }

    if (swipeWasCommenced)
        return;

    static auto* const* PDISTANCE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gesture_distance")->getDataStaticPtr();
    const double        distance  = std::max<Hyprlang::INT>(1, **PDISTANCE);

    const float         PERC               = closing ? std::clamp(delta / distance, 0.0, 1.0) : 1.0 - std::clamp(delta / distance, 0.0, 1.0);
    const auto          WORKSPACE_FOCUS_ID = closing && closeOnID != -1 ? closeOnID : openedID;

    Vector2D            tileSize = (MON->m_size / SIDE_LENGTH);

    const auto          SIZEMAX = MON->m_size * MON->m_size / tileSize;
    const auto          POSMAX  = (-((MON->m_size / (double)SIDE_LENGTH) * Vector2D{WORKSPACE_FOCUS_ID % SIDE_LENGTH, WORKSPACE_FOCUS_ID / SIDE_LENGTH}) * MON->m_scale) *
        (MON->m_size / tileSize);

    const auto SIZEMIN = MON->m_size;
    const auto POSMIN  = Vector2D{0, 0};

    size->setCallbackOnEnd(nullptr);
    pos->setCallbackOnEnd(nullptr);

    size->setValueAndWarp(lerp(SIZEMIN, SIZEMAX, PERC));
    pos->setValueAndWarp(lerp(POSMIN, POSMAX, PERC));
}

void COverview::onSwipeEnd() {
    if (m_closeCommitted)
        return;

    const auto MON = pMonitor.lock();
    if (!MON) {
        m_isSwiping       = false;
        swipeWasCommenced = false;
        closing           = true;
        g_pOverview.reset();
        return;
    }

    const auto SIZEMIN = MON->m_size;
    const auto SIZEMAX = MON->m_size * MON->m_size / (MON->m_size / SIDE_LENGTH);
    const auto PERC    = (size->value() - SIZEMIN).x / (SIZEMAX - SIZEMIN).x;
    if (PERC > 0.5) {
        close();
        return;
    }
    *size = MON->m_size;
    *pos  = {0, 0};

    size->setCallbackOnEnd([this](WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) { redrawAll(true); });

    swipeWasCommenced = true;
    m_isSwiping       = false;
}

void COverview::enterSubmapIfEnabled() {
    static auto* const* PKEYNAV = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:keynav_enable")->getDataStaticPtr();
    if (**PKEYNAV && !submapActive) {
        // switch to a dedicated submap for hyprexpo navigation
        g_pKeybindManager->m_dispatchers["submap"]("hyprexpo");
        submapActive = true;
    }
}

void COverview::resetSubmapIfNeeded() {
    if (submapActive) {
        g_pKeybindManager->m_dispatchers["submap"]("reset");
        submapActive = false;
    }
}
