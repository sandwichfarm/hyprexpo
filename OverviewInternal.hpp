#pragma once

#include "Overview.hpp"

#include <string>
#include <utility>
#include <vector>

struct SHyprGradientSpec {
    CHyprColor c1;
    CHyprColor c2;
    float      angleDeg = 0.f;
    bool       valid    = false;
};

struct SWorkspacePreviewState {
    bool     visible        = false;
    bool     forceRendering = false;
    float    alphaValue     = 1.F;
    float    alphaGoal      = 1.F;
    Vector2D offsetValue;
    Vector2D offsetGoal;
};

struct SWindowPreviewState {
    PHLWINDOW window;
    Vector2D  positionValue;
    Vector2D  positionGoal;
    Vector2D  sizeValue;
    Vector2D  sizeGoal;
};

void clearWithColor(const CHyprColor& color);
uint32_t framebufferFormatWithAlpha(uint32_t drmFormat);
bool isTransformRotated(wl_output_transform t);

std::string trimString(std::string value);
std::vector<std::string> splitCommaList(const std::string& value);
std::string lowerString(std::string value);
std::string fallbackTokenForVisibleIndex(int visibleIndex);
int fallbackTokenToVisibleIndex(const std::string& token);

SHyprGradientSpec parseGradientSpec(const std::string& inRaw);
bool isGradientBorderSpec(const std::string& borderSpec);
SP<Render::ITexture> renderNumberTexture(const std::string& text, const CHyprColor& color, const Vector2D& bufferSize, float scale, int fontSize);

SWorkspacePreviewState applyWorkspacePreviewState(const PHLWORKSPACE& workspace);
void restoreWorkspacePreviewState(const PHLWORKSPACE& workspace, const SWorkspacePreviewState& state);
std::vector<std::pair<PHLWORKSPACE, SWorkspacePreviewState>> applyExclusiveWorkspacePreviewState(const PHLWORKSPACE& workspace);
void restoreWorkspacePreviewStates(const std::vector<std::pair<PHLWORKSPACE, SWorkspacePreviewState>>& states);
void normalizeMonitorWorkspaceRenderState(PHLMONITOR monitor);

bool windowVisibleOnWorkspace(const PHLWINDOW& window, const PHLWORKSPACE& workspace);
void settleWorkspaceMoveAnimation(const PHLWINDOW& window);
void settleWorkspaceMoveAnimations();
void ensureFramebuffer(COverview::SWorkspaceImage& image, const CBox& monbox, uint32_t drmFormat);
std::vector<SWindowPreviewState> applyWorkspaceWindowGoalState(const PHLWORKSPACE& workspace);
void restoreWorkspaceWindowGoalState(const std::vector<SWindowPreviewState>& states);
PHLWORKSPACE activateWorkspaceForPreview(PHLMONITOR monitor, const PHLWORKSPACE& workspace);
void restoreActiveWorkspaceAfterPreview(PHLMONITOR monitor, const PHLWORKSPACE& workspace);
void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr);
