#pragma once

#include "IOverviewSession.hpp"
#include "ScrollingLayoutAdapter.hpp"
#include "ScrollingInputState.hpp"
#include "ScrollingOverviewLogic.hpp"

#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopTimer.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/render/Texture.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CScrollingOverview final : public IOverviewSession {
  public:
    CScrollingOverview(const PHLWORKSPACE& startedOn, bool swipe, uint64_t sessionGeneration, const std::optional<Hyprexpo::Scrolling::SWorkspaceSnapshot>& initialSnapshot);
    ~CScrollingOverview() override;

    bool valid() const;

    void render() override;
    void damage() override;
    void onDamageReported() override;
    void onPreRender() override;
    void onConfigReload() override;
    void prepareForTeardown() override;
    std::expected<std::string, std::string> injectScrollingInput(const std::string& sequence) override;
    void fullRender() override;
    void setClosing(bool closing) override;
    void beginCancelSwipe() override;
    bool closeCommitted() const override;
    bool shouldRenderOverviewForMonitor(const PHLMONITOR& monitor) const override;
    void onWindowMoveToWorkspace(const PHLWINDOW& window, const PHLWORKSPACE& workspace) override;
    void resetSwipe() override;
    void onSwipeUpdate(double delta) override;
    void onSwipeEnd(bool switchToSelection) override;
    void close(bool switchToSelection = true) override;
    void selectHoveredWorkspace() override;
    void onKbMoveFocus(const std::string& direction) override;
    void onKbConfirm() override;
    void onKbSelectNumber(int number) override;
    void onKbSelectToken(int visibleIndex) override;
    bool selectVisibleToken(const std::string& token) override;
    int64_t selectedWorkspaceID() const override;
    bool selectWorkspaceByID(int64_t workspaceID) override;
    bool selectVisibleIndex(size_t index) override;
    bool moveWindowBetweenVisibleIndices(size_t sourceIndex, size_t targetIndex, const PHLWINDOW& window = nullptr) override;
    bool blocksOverviewRendering() const override;
    bool blocksDamageReporting() const override;
    bool isSwiping() const override;
    PHLMONITOR monitor() const override;
    uint64_t sessionGeneration() const override;

  private:
    struct SCacheKey {
        uint64_t sessionGeneration       = 0;
        int64_t  workspaceID             = 0;
        uint64_t targetToken             = 0;
        uint64_t contentDamageGeneration = 0;
        uint32_t captureWidth             = 0;
        uint32_t captureHeight            = 0;
        uint64_t budgetGeneration         = 0;

        bool operator==(const SCacheKey&) const = default;
    };

    struct SCacheEntry {
        SCacheKey                   key;
        WP<Layout::ITarget>         targetRef;
        PHLWINDOWREF                windowRef;
        SP<Render::IFramebuffer>    framebuffer;
        SP<Render::ITexture>        texture;
    };

    struct SWorkspaceRow {
        PHLWORKSPACE                             workspace;
        Hyprexpo::Scrolling::EWorkspaceKind      kind = Hyprexpo::Scrolling::EWorkspaceKind::Empty;
        Hyprexpo::Scrolling::SWorkspaceSnapshot  snapshot;
        SP<Render::IFramebuffer>                  workspacePreview;
    };

    struct SRenderTarget {
        int64_t            workspaceID = 0;
        uint64_t           targetToken = 0;
        uint64_t           windowStableID = 0;
        CBox               box;
        WP<Layout::ITarget> targetRef;
        PHLWINDOWREF       windowRef;
        bool               group = false;
        bool               floating = false;
        bool               fullscreen = false;
        bool               pinned = false;
    };

    bool refreshScene(const std::optional<Hyprexpo::Scrolling::SWorkspaceSnapshot>& initialSnapshot = std::nullopt);
    void refreshCache();
    void releaseCacheEntry(SCacheEntry& entry);
    void releaseAllCaptureState();
    SCacheEntry* cacheEntry(int64_t workspaceID, uint64_t targetToken);
    const SWorkspaceRow* workspaceRow(int64_t workspaceID) const;
    void updateSelectionFromFocus();
    void ensureFocusVisible();
    bool commitSelection();
    void installInputListeners();
    Hyprexpo::Scrolling::SInputEffects resetInputState(Hyprexpo::Scrolling::EResetReason reason);
    Hyprexpo::Scrolling::SInputContext inputContext() const;
    Hyprexpo::Scrolling::SInputEffects processInput(const Hyprexpo::Scrolling::SInputEvent& event);
    void applyInputEffects(const Hyprexpo::Scrolling::SInputEffects& effects, const Hyprexpo::Scrolling::SInputState& previousState);
    std::optional<Hyprexpo::SPoint> normalizeTouchPoint(Hyprexpo::SPoint normalizedPoint, const PHLMONITOR& touchedMonitor) const;

    PHLWORKSPACE                              m_startedOn;
    PHLMONITORREF                             m_monitor;
    uint64_t                                  m_sessionGeneration = 0;
    uint64_t                                  m_contentDamageGeneration = 0;
    uint64_t                                  m_budgetGeneration = 0;
    uint64_t                                  m_mutationRequestSequence = 0;
    bool                                      m_valid = false;
    bool                                      m_closing = false;
    bool                                      m_closeCommitted = false;
    bool                                      m_blockOverviewRendering = false;
    bool                                      m_blockDamageReporting = false;
    bool                                      m_isSwiping = false;
    bool                                      m_swipeClosing = false;
    bool                                      m_damageDirty = false;
    bool                                      m_showPinnedWindows = false;
    double                                    m_pan = 0.0;
    double                                    m_swipeDelta = 0.0;
    PHLANIMVAR<float> m_transitionProgress;
    SP<CEventLoopTimer>                       m_closeAnimationTimer;
    int64_t                                   m_selectedWorkspaceID = 0;
    uint64_t                                  m_selectedStableID = 0;
    WP<Layout::ITarget>                       m_selectedTarget;
    PHLWINDOWREF                              m_selectedWindow;
    Hyprexpo::Scrolling::SFocusRef            m_focus;
    Hyprexpo::Scrolling::SScene               m_scene;
    std::vector<SWorkspaceRow>                m_rows;
    std::vector<SRenderTarget>                m_renderTargets;
    std::vector<SCacheEntry>                  m_cache;
    Hyprexpo::Scrolling::SInputState          m_inputState;
    Hyprexpo::Scrolling::SDropSource          m_pendingDropSource;
    std::optional<Hyprexpo::Scrolling::SDropIntent> m_pendingDropIntent;
    PHLMONITORREF                             m_touchMonitor;
    CHyprSignalListener                       mouseMoveHook;
    CHyprSignalListener                       mouseButtonHook;
    CHyprSignalListener                       mouseAxisHook;
    CHyprSignalListener                       touchDownHook;
    CHyprSignalListener                       touchMotionHook;
    CHyprSignalListener                       touchUpHook;
    CHyprSignalListener                       touchCancelHook;
};
