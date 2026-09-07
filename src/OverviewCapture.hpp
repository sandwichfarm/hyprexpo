#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/render/Texture.hpp>

#include <string>

namespace Hyprexpo::Capture {

struct SWorkspaceCaptureRequest {
    PHLMONITOR   monitor;
    PHLWORKSPACE workspace;
    PHLWORKSPACE startedOn;
    CBox         box;
    bool         showPinnedWindows      = false;
    bool         blockSurfaceFeedback   = false;
    bool         animateStartedOnRestore = false;
};

struct SWindowCaptureResult {
    SP<Render::IFramebuffer> framebuffer;
    SP<Render::ITexture>     texture;
    std::string              error;
    bool                     completed = false;
};

bool captureWorkspacePreview(const SWorkspaceCaptureRequest& request, SP<Render::IFramebuffer>& framebuffer);

SWindowCaptureResult captureWindowPreview(const WP<Layout::ITarget>& target, const PHLWINDOWREF& window, const PHLMONITORREF& monitor, const Vector2D& pixelSize);

int      scrollingThumbnailBudgetMultiplier();
uint64_t scrollingThumbnailBudgetGeneration();
void     notifyOverviewCaptureConfigReload();

}
