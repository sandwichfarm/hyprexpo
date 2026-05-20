#include "HyprlandConfigCompat.hpp"
#define HyprlandAPI CompatHyprlandAPI
#include "OverviewInternal.hpp"
#include "HyprexpoLogic.hpp"
#include "OverviewPassElement.hpp"
#define private   public
#define protected public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#undef private
#undef protected
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

void COverview::redrawID(int id, bool forcelowres) {
    if (!pMonitor)
        return;

    if (pMonitor->m_activeWorkspace != startedOn && !closing) {
        // likely user changed.
        onWorkspaceChange();
    }

    blockOverviewRendering = true;

    Render::GL::g_pHyprOpenGL->makeEGLCurrent();

    id = std::clamp(id, 0, SIDE_LENGTH * SIDE_LENGTH - 1);

    Vector2D tileSize       = pMonitor->m_size / SIDE_LENGTH;
    Vector2D tileRenderSize = (pMonitor->m_size - Vector2D{GAP_WIDTH, GAP_WIDTH} * (SIDE_LENGTH - 1)) / SIDE_LENGTH;
    CBox     monbox{0, 0, tileSize.x * 2, tileSize.y * 2};

    if (!forcelowres && (size->value() != pMonitor->m_size || closing))
        monbox = {{0, 0}, pMonitor->m_pixelSize};

    if (!ENABLE_LOWRES)
        monbox = {{0, 0}, pMonitor->m_pixelSize};

    const auto savedTransform       = pMonitor->m_transform;
    const auto savedTransformedSize = pMonitor->m_transformedSize;
    const auto savedPixelSize       = pMonitor->m_pixelSize;

    // Fix for rotated monitors: swap dimensions to match logical orientation
    if (isTransformRotated(savedTransform)) {
        monbox = {{0, 0}, {monbox.h, monbox.w}};

        // Override monitor state to disable rotation
        pMonitor->m_transform       = WL_OUTPUT_TRANSFORM_NORMAL;
        pMonitor->m_pixelSize       = {monbox.w, monbox.h};
        pMonitor->m_transformedSize = {monbox.w, monbox.h};
    }

    auto& image = images[id];

    ensureFramebuffer(image, monbox, framebufferFormatWithAlpha(pMonitor->m_output->state->state().drmFormat));

    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, Render::RENDER_MODE_FULL_FAKE, nullptr, image.fb);

    clearWithColor(CHyprColor{0, 0, 0, 1.0});

    const auto PWORKSPACE = image.pWorkspace ? image.pWorkspace : g_pCompositor->getWorkspaceByID(image.workspaceID);
    image.pWorkspace      = PWORKSPACE;

    PHLWORKSPACE openSpecial = pMonitor->m_activeSpecialWorkspace;
    if (openSpecial)
        pMonitor->m_activeSpecialWorkspace.reset();

    startedOn->m_visible = false;

    if (PWORKSPACE) {
        const auto previousWS    = activateWorkspaceForPreview(pMonitor.lock(), PWORKSPACE);
        const auto previewState  = applyWorkspacePreviewState(PWORKSPACE);
        const auto windowState   = PWORKSPACE == startedOn ? std::vector<SWindowPreviewState>{} : applyWorkspaceWindowGoalState(PWORKSPACE);

        if (PWORKSPACE == startedOn)
            pMonitor->m_activeSpecialWorkspace = openSpecial;

        g_pHyprRenderer->renderWorkspace(pMonitor.lock(), PWORKSPACE, Time::steadyNow(), monbox);

        restoreWorkspaceWindowGoalState(windowState);
        restoreWorkspacePreviewState(PWORKSPACE, previewState);
        restoreActiveWorkspaceAfterPreview(pMonitor.lock(), previousWS);

        if (PWORKSPACE == startedOn)
            pMonitor->m_activeSpecialWorkspace.reset();
    } else
        g_pHyprRenderer->renderWorkspace(pMonitor.lock(), PWORKSPACE, Time::steadyNow(), monbox);

    g_pHyprRenderer->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    // Restore the original monitor state after capture
    pMonitor->m_transform       = savedTransform;
    pMonitor->m_pixelSize       = savedPixelSize;
    pMonitor->m_transformedSize = savedTransformedSize;

    pMonitor->m_activeSpecialWorkspace = openSpecial;
    pMonitor->m_activeWorkspace        = startedOn;
    startedOn->m_visible               = true;
    g_pDesktopAnimationManager->startAnimation(startedOn, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);

    blockOverviewRendering = false;
}

void COverview::redrawAll(bool forcelowres) {
    if (!pMonitor)
        return;

    for (size_t i = 0; i < (size_t)(SIDE_LENGTH * SIDE_LENGTH); ++i) {
        redrawID(i, forcelowres);
    }
}

void COverview::damage() {
    blockDamageReporting = true;
    g_pHyprRenderer->damageMonitor(pMonitor.lock());
    blockDamageReporting = false;
}

void COverview::onDamageReported() {
    damageDirty = true;

    Vector2D SIZE = size->value();

    Vector2D tileSize       = (SIZE / SIDE_LENGTH);
    const auto GAPSIZE      = (closing ? (1.0 - size->getPercent()) : size->getPercent()) * GAP_WIDTH;
    static auto* const* PGAPSO = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gaps_out")->getDataStaticPtr();
    const float OUTER       = std::max<Hyprlang::INT>(0, **PGAPSO) * (closing ? (1.0 - size->getPercent()) : size->getPercent());
    Vector2D tileRenderSize = (SIZE - Vector2D{GAPSIZE, GAPSIZE} * (SIDE_LENGTH - 1) - Vector2D{OUTER * 2, OUTER * 2}) / SIDE_LENGTH;
    // const auto& TILE           = images[std::clamp(openedID, 0, SIDE_LENGTH * SIDE_LENGTH)];
    CBox texbox = CBox{OUTER + (openedID % SIDE_LENGTH) * tileRenderSize.x + (openedID % SIDE_LENGTH) * GAPSIZE,
                       OUTER + (openedID / SIDE_LENGTH) * tileRenderSize.y + (openedID / SIDE_LENGTH) * GAPSIZE, tileRenderSize.x, tileRenderSize.y}
                      .translate(pMonitor->m_position);

    damage();

    blockDamageReporting = true;
    g_pHyprRenderer->damageBox(texbox);
    blockDamageReporting = false;
    g_pCompositor->scheduleFrameForMonitor(pMonitor.lock());
}

void COverview::close(bool switchToSelection) {
    if (closing)
        return;

    resetSubmapIfNeeded();

    const int   ID = closeOnID == -1 ? openedID : closeOnID;

    const int   SAFEID = std::clamp(ID, 0, SIDE_LENGTH * SIDE_LENGTH - 1);
    const auto& TILE   = images[SAFEID];

    Vector2D    tileSize = (pMonitor->m_size / SIDE_LENGTH);

    *size = pMonitor->m_size * pMonitor->m_size / tileSize;
    *pos  = (-((pMonitor->m_size / (double)SIDE_LENGTH) * Vector2D{SAFEID % SIDE_LENGTH, SAFEID / SIDE_LENGTH}) * pMonitor->m_scale) * (pMonitor->m_size / tileSize);

    size->setCallbackOnEnd(removeOverview);

    closing = true;

    redrawAll();

    if (switchToSelection && TILE.workspaceID != pMonitor->activeWorkspaceID()) {
        pMonitor->setSpecialWorkspace(0);

        // If this tile's workspace was WORKSPACE_INVALID, move to the next
        // empty workspace. This should only happen if skip_empty is on, in
        // which case some tiles will be left with this ID intentionally.
        const int  NEWID = TILE.workspaceID == WORKSPACE_INVALID ? getWorkspaceIDNameFromString("emptynm").id : TILE.workspaceID;

        const auto NEWIDWS = g_pCompositor->getWorkspaceByID(NEWID);

        const auto OLDWS = pMonitor->m_activeWorkspace;

        const auto CHANGE = !NEWIDWS ? Config::Actions::changeWorkspace(std::to_string(NEWID)) : Config::Actions::changeWorkspace(NEWIDWS->getConfigName());
        if (!CHANGE)
            Log::logger->log(Log::ERR, "[hyprexpo] failed to change workspace: {}", CHANGE.error().message);

        g_pDesktopAnimationManager->startAnimation(pMonitor->m_activeWorkspace, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);
        g_pDesktopAnimationManager->startAnimation(OLDWS, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);

        startedOn = pMonitor->m_activeWorkspace;
    }
}

void COverview::onPreRender() {
    if (damageDirty) {
        damageDirty = false;
        redrawID(closing ? (closeOnID == -1 ? openedID : closeOnID) : openedID);
    }
}

void COverview::onWorkspaceChange() {
    if (valid(startedOn))
        g_pDesktopAnimationManager->startAnimation(startedOn, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);
    else
        startedOn = pMonitor->m_activeWorkspace;

    for (size_t i = 0; i < (size_t)(SIDE_LENGTH * SIDE_LENGTH); ++i) {
        if (images[i].workspaceID != pMonitor->activeWorkspaceID())
            continue;

        openedID = i;
        break;
    }

    closeOnID = openedID;
    close();
}

void COverview::render() {
    g_pHyprRenderer->m_renderPass.add(makeUnique<COverviewPassElement>());
}

void COverview::fullRender() {
    const auto GAPSIZE = (closing ? (1.0 - size->getPercent()) : size->getPercent()) * GAP_WIDTH;

    if (pMonitor->m_activeWorkspace != startedOn && !closing) {
        // likely user changed.
        onWorkspaceChange();
    }

    Vector2D SIZE = size->value();

    static auto* const* PGAPSO = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gaps_out")->getDataStaticPtr();
    const float OUTER = std::max<Hyprlang::INT>(0, **PGAPSO) * (closing ? (1.0 - size->getPercent()) : size->getPercent());

    Vector2D tileSize       = (SIZE / SIDE_LENGTH);
    Vector2D tileRenderSize = (SIZE - Vector2D{GAPSIZE, GAPSIZE} * (SIDE_LENGTH - 1) - Vector2D{OUTER * 2, OUTER * 2}) / SIDE_LENGTH;

    clearWithColor(BG_COLOR.stripA());

    static auto* const* PTILEROUND = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding")->getDataStaticPtr();
    static auto* const* PTOUNDPWR  = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_power")->getDataStaticPtr();
    static auto* const* PTILEROUNDF = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_focus")->getDataStaticPtr();
    static auto* const* PTILEROUNDC = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_current")->getDataStaticPtr();
    static auto* const* PTILEROUNDH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_hover")->getDataStaticPtr();

    const int BASE_ROUND_SCALED   = std::max(0, (int)std::lround((double)**PTILEROUND * pMonitor->m_scale));
    const int FOCUS_ROUND_SCALED  = **PTILEROUNDF >= 0 ? std::max(0, (int)std::lround((double)**PTILEROUNDF * pMonitor->m_scale)) : BASE_ROUND_SCALED;
    const int CURRENT_ROUND_SCALED= **PTILEROUNDC >= 0 ? std::max(0, (int)std::lround((double)**PTILEROUNDC * pMonitor->m_scale)) : BASE_ROUND_SCALED;
    const int HOVER_ROUND_SCALED  = **PTILEROUNDH >= 0 ? std::max(0, (int)std::lround((double)**PTILEROUNDH * pMonitor->m_scale)) : BASE_ROUND_SCALED;
    const float ROUND_PWR         = **PTOUNDPWR;

    // (shadows moved to feature/shadows branch)

    std::vector<CBox> tileBoxes(images.size());

    for (size_t y = 0; y < (size_t)SIDE_LENGTH; ++y) {
        for (size_t x = 0; x < (size_t)SIDE_LENGTH; ++x) {
            const int id = x + y * SIDE_LENGTH;
            CBox      texbox{OUTER + x * tileRenderSize.x + x * GAPSIZE, OUTER + y * tileRenderSize.y + y * GAPSIZE, tileRenderSize.x, tileRenderSize.y};
            texbox.scale(pMonitor->m_scale).translate(pos->value());
            texbox.round();
            tileBoxes[id] = texbox;
            // per-tile rounding override for focus/current/hover (priority: focus > current > hover)
            int tileRound = BASE_ROUND_SCALED;
            if ((int)id == kbFocusID)
                tileRound = FOCUS_ROUND_SCALED;
            else if ((int)id == openedID)
                tileRound = CURRENT_ROUND_SCALED;
            else if ((int)id == hoveredID)
                tileRound = HOVER_ROUND_SCALED;

            // clamp rounding to tile size
            const int maxCornerPx = std::max(0, (int)std::floor(std::min(texbox.w, texbox.h) / 2.0));
            tileRound = std::min(tileRound, maxCornerPx);

            // no shadow in this branch

            CRegion damage{0, 0, INT16_MAX, INT16_MAX};
            Render::GL::g_pHyprOpenGL->renderTextureInternal(images[id].fb->getTexture(), texbox, {.damage = &damage, .a = 1.0, .round = tileRound, .roundingPower = ROUND_PWR});
        }
    }

    // overlays: numbers and borders
    static auto* const* PLABELEN   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_enable")->getDataStaticPtr();
    static auto* const* PLABELSIZE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_font_size")->getDataStaticPtr();
    static auto  const* PLABELPOS  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_position")->getDataStaticPtr();
    static auto  const* PLABELMODE = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_text_mode")->getDataStaticPtr();
    static auto  const* PTOKENMAP  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_token_map")->getDataStaticPtr();
    static auto* const* PLABELOX   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_offset_x")->getDataStaticPtr();
    static auto* const* PLABELOY   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_offset_y")->getDataStaticPtr();
    static auto  const* PLABELSHOW = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_show")->getDataStaticPtr();
    static auto* const* PLCOLDEF   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_color_default")->getDataStaticPtr();
    static auto* const* PLCOLHOV   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_color_hover")->getDataStaticPtr();
    static auto* const* PLCOLFOC   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_color_focus")->getDataStaticPtr();
    static auto* const* PLCOLCUR   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_color_current")->getDataStaticPtr();
    static auto* const* PWSNUMCOL  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:workspace_number_color")->getDataStaticPtr();
    static auto* const* PLSCALEH   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_scale_hover")->getDataStaticPtr();
    static auto* const* PLSCALEF   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_scale_focus")->getDataStaticPtr();
    static auto* const* PLBGEN     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_enable")->getDataStaticPtr();
    static auto* const* PLBGCOL    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_color")->getDataStaticPtr();
    static auto* const* PLBGROUND  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_rounding")->getDataStaticPtr();
    static auto  const* PLBGSHAPE  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_shape")->getDataStaticPtr();
    static auto* const* PLBGPAD    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_padding")->getDataStaticPtr();

    static auto* const* PBWIDTH      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_width")->getDataStaticPtr();
    static auto  const* PBCOLCUR     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_color_current")->getDataStaticPtr();
    static auto  const* PBCOLFOC     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_color_focus")->getDataStaticPtr();
    static auto  const* PBCOLHOV     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_color_hover")->getDataStaticPtr();
    // Deprecated configs for backwards compatibility
    static auto  const* PBGRCUR      = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_grad_current")->getDataStaticPtr();
    static auto  const* PBGREFOC     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_grad_focus")->getDataStaticPtr();
    static auto  const* PBGREHOV     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_grad_hover")->getDataStaticPtr();

    static auto* const* PDRAGPROXYCOL     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:drag_drop_proxy_color")->getDataStaticPtr();
    static auto* const* PDRAGPROXYACTCOL  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:drag_drop_proxy_active_color")->getDataStaticPtr();
    static auto  const* PDRAGPROXYBORDER  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:drag_drop_proxy_border_color")->getDataStaticPtr();
    static auto* const* PDRAGPROXYBWIDTH  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:drag_drop_proxy_border_width")->getDataStaticPtr();
    static auto* const* PDRAGPROXYROUND   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:drag_drop_proxy_rounding")->getDataStaticPtr();
    static auto  const* PDRAGSOURCEBORDER = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:drag_drop_source_border_color")->getDataStaticPtr();
    static auto* const* PDRAGSOURCEBWIDTH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:drag_drop_source_border_width")->getDataStaticPtr();

    static auto* const* PSELECTEN   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:selection_label_enable")->getDataStaticPtr();
    static auto  const* PSELECTMAP  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:selection_label_token_map")->getDataStaticPtr();
    static auto  const* PSELECTPOS  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:selection_label_position")->getDataStaticPtr();
    static auto* const* PSELECTOX   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:selection_label_offset_x")->getDataStaticPtr();
    static auto* const* PSELECTOY   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:selection_label_offset_y")->getDataStaticPtr();
    static auto* const* PSELECTCOL  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:selection_label_color")->getDataStaticPtr();

    // draw labels
    if (**PLABELEN || **PSELECTEN || showWorkspaceNumbers) {
        // use the tracked hoveredID (cleared during closing)
        const int labelHoveredID = closing ? -1 : hoveredID;

        auto shouldShow = [&](int id) -> bool {
            if (showWorkspaceNumbers)
                return true;
            if (std::string{*PLABELSHOW} == "never")
                return false;
            if (std::string{*PLABELSHOW} == "always")
                return true;
            const bool isHover  = id == labelHoveredID;
            const bool isFocus  = id == kbFocusID;
            const bool isCurr   = id == openedID;
            const std::string mode{*PLABELSHOW};
            if (mode == "hover")
                return isHover;
            if (mode == "focus")
                return isFocus;
            if (mode == "hover+focus")
                return isHover || isFocus;
            if (mode == "current+focus")
                return isCurr || isFocus;
            return true;
        };

        auto resolveState = [&](int id) -> int {
            // precedence: focus > current > hover > default
            if (id == kbFocusID)
                return 2; // focus
            if (id == openedID)
                return 3; // current
            if (id == labelHoveredID)
                return 1; // hover
            return 0;     // default
        };

        auto placeBox = [&](const CBox& tile, const Vector2D& size, const std::string& anchor, int offsetX, int offsetY) -> CBox {
            double x = tile.x, y = tile.y;
            if (anchor == "top-left") {
                x += offsetX; y += offsetY;
            } else if (anchor == "top-right") {
                x += tile.w - size.x - offsetX; y += offsetY;
            } else if (anchor == "bottom-left") {
                x += offsetX; y += tile.h - size.y - offsetY;
            } else if (anchor == "bottom-right") {
                x += tile.w - size.x - offsetX; y += tile.h - size.y - offsetY;
            } else { // center
                x += (tile.w - size.x) / 2.0; y += (tile.h - size.y) / 2.0;
            }
            return CBox{x, y, (double)size.x, (double)size.y};
        };

        auto renderLabel = [&](SP<Render::ITexture>& tex, Vector2D& sz, const std::string& label, const CHyprColor& col, float scaleMul, const CBox& tile, const std::string& anchor,
                               int offsetX, int offsetY) {
            if (label.empty())
                return;

            const int baseF = std::max(8, (int)**PLABELSIZE);
            if (!tex || tex->m_texID == 0) {
                const int fsz = std::max(8, (int)std::round(baseF * scaleMul));
                Vector2D  buf{std::max(32, fsz * 2), std::max(24, fsz + 8)};
                sz  = buf;
                tex = renderNumberTexture(label, col, buf, pMonitor->m_scale, fsz);
            }

            if (!tex || tex->m_texID == 0)
                return;

            static auto* const* PLPIXELSNAP = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_pixel_snap")->getDataStaticPtr();

            auto drawWithBG = [&]() {
                const int pad = **PLBGPAD;
                Vector2D  bgSize = {sz.x + pad * 2, sz.y + pad * 2};
                const std::string shape{*PLBGSHAPE};
                int roundPx = **PLBGROUND;
                if (shape == "circle" || shape == "square") {
                    const double side = std::max(bgSize.x, bgSize.y);
                    bgSize            = {side, side};
                    roundPx           = (shape == "circle") ? std::lround(side / 2.0) : 0;
                }
                CBox bg = placeBox(tile, bgSize, anchor, offsetX, offsetY);
                CBox lb{bg.x + (bg.w - sz.x) / 2.0, bg.y + (bg.h - sz.y) / 2.0, (double)sz.x, (double)sz.y};
                if (**PLPIXELSNAP) {
                    bg.round();
                    lb.round();
                }
                Render::GL::g_pHyprOpenGL->renderRect(bg, CHyprColor{(uint64_t)**PLBGCOL}, {.round = roundPx});
                Render::GL::g_pHyprOpenGL->renderTexture(tex, lb, {.a = 1.0});
            };

            auto drawNoBG = [&]() {
                CBox lb = placeBox(tile, sz, anchor, offsetX, offsetY);
                if (**PLPIXELSNAP)
                    lb.round();
                Render::GL::g_pHyprOpenGL->renderTexture(tex, lb, {.a = 1.0});
            };

            if (**PLBGEN)
                drawWithBG();
            else
                drawNoBG();
        };

        std::vector<std::string> labelTokens;
        if (!std::string{*PTOKENMAP}.empty())
            labelTokens = splitCommaList(std::string{*PTOKENMAP});

        std::vector<std::string> selectionTokens;
        if (!std::string{*PSELECTMAP}.empty())
            selectionTokens = splitCommaList(std::string{*PSELECTMAP});

        int tokenCounter = 0;
        for (size_t y = 0; y < (size_t)SIDE_LENGTH; ++y) {
            for (size_t x = 0; x < (size_t)SIDE_LENGTH; ++x) {
                const int id = x + y * SIDE_LENGTH;
                if (images[id].workspaceID == WORKSPACE_INVALID)
                    continue;

                // compute tile box again for label placement
                CBox tile{OUTER + x * tileRenderSize.x + x * GAPSIZE, OUTER + y * tileRenderSize.y + y * GAPSIZE, tileRenderSize.x, tileRenderSize.y};
                tile.scale(pMonitor->m_scale).translate(pos->value());
                tile.round();

                if ((**PLABELEN || showWorkspaceNumbers) && shouldShow(id)) {
                    std::string label;
                    const std::string mode = showWorkspaceNumbers ? std::string{"id"} : std::string{*PLABELMODE};
                    if (mode == "token") {
                        if (tokenCounter < (int)labelTokens.size() && !labelTokens[tokenCounter].empty())
                            label = labelTokens[tokenCounter];
                        else
                            label = fallbackTokenForVisibleIndex(tokenCounter);
                    } else if (mode == "index") {
                        label = std::to_string(tokenCounter + 1);
                    } else {
                        label = std::to_string(images[id].workspaceID);
                    }

                    const int st = resolveState(id);
                    if (!label.empty()) {
                        if (showWorkspaceNumbers)
                            renderLabel(images[id].labelTexDefault, images[id].labelSizeDefault, label, CHyprColor{(uint64_t)**PWSNUMCOL}, 1.0f, tile, std::string{*PLABELPOS}, **PLABELOX, **PLABELOY);
                        else if (st == 1)
                            renderLabel(images[id].labelTexHover, images[id].labelSizeHover, label, CHyprColor{(uint64_t)**PLCOLHOV}, **PLSCALEH, tile, std::string{*PLABELPOS}, **PLABELOX, **PLABELOY);
                        else if (st == 2)
                            renderLabel(images[id].labelTexFocus, images[id].labelSizeFocus, label, CHyprColor{(uint64_t)**PLCOLFOC}, **PLSCALEF, tile, std::string{*PLABELPOS}, **PLABELOX, **PLABELOY);
                        else if (st == 3)
                            renderLabel(images[id].labelTexCurrent, images[id].labelSizeCurrent, label, CHyprColor{(uint64_t)**PLCOLCUR}, 1.0f, tile, std::string{*PLABELPOS}, **PLABELOX, **PLABELOY);
                        else
                            renderLabel(images[id].labelTexDefault, images[id].labelSizeDefault, label, CHyprColor{(uint64_t)**PLCOLDEF}, 1.0f, tile, std::string{*PLABELPOS}, **PLABELOX, **PLABELOY);
                    }
                }

                if (**PSELECTEN && tokenCounter < (int)selectionTokens.size() && !selectionTokens[tokenCounter].empty())
                    renderLabel(images[id].selectionLabelTex, images[id].selectionLabelSize, selectionTokens[tokenCounter], CHyprColor{(uint64_t)**PSELECTCOL}, 1.0f, tile,
                                std::string{*PSELECTPOS}, **PSELECTOX, **PSELECTOY);

                tokenCounter++;
            }
        }
    }

    // draw borders for hover, current and focus (priority order: focus > current > hover)

    // pass rounding based on state
    const int RND_CUR = CURRENT_ROUND_SCALED;
    const int RND_FOC = FOCUS_ROUND_SCALED;
    const int RND_HOV = HOVER_ROUND_SCALED;

    // Helper to parse border config (supports rgb/hex/gradient, with deprecated fallback)
    auto drawBorderForID = [&](int id, const std::string& borderSpec, const std::string& deprecatedGradSpec, int roundScaled, int borderWidthOverride = -1) {
        if (id < 0)
            return;
        if (borderWidthOverride == 0)
            return;
        const int ix = id % SIDE_LENGTH;
        const int iy = id / SIDE_LENGTH;
        CBox       box{OUTER + ix * tileRenderSize.x + ix * GAPSIZE, OUTER + iy * tileRenderSize.y + iy * GAPSIZE, tileRenderSize.x, tileRenderSize.y};
        box.scale(pMonitor->m_scale).translate(pos->value());
        box.round();
        const int BWIDTH = std::max(1, borderWidthOverride > 0 ? borderWidthOverride : (int)**PBWIDTH);

        // Determine which spec to use (prefer new format, fallback to deprecated)
        std::string effectiveSpec = borderSpec.empty() ? deprecatedGradSpec : borderSpec;

        // Auto-detect format: gradient vs solid color
        if (isGradientBorderSpec(effectiveSpec)) {
            // Render as gradient border (hyprland style)
            const auto spec = parseGradientSpec(effectiveSpec);
            if (spec.valid) {
                Config::CGradientValueData grad;
                grad.m_colors.clear();
                grad.m_colors.push_back(spec.c1);
                grad.m_colors.push_back(spec.c2);
                grad.m_angle = spec.angleDeg * (float)M_PI / 180.f;
                grad.updateColorsOk();
                Render::GL::g_pHyprOpenGL->renderBorder(box, grad, {.round = roundScaled, .roundingPower = ROUND_PWR, .borderSize = BWIDTH});
            }
        } else if (!effectiveSpec.empty()) {
            Hyprexpo::SColorRGBA parsedColor;
            if (Hyprexpo::parseSolidColorSpec(effectiveSpec, parsedColor)) {
                CHyprColor color{parsedColor.r, parsedColor.g, parsedColor.b, parsedColor.a};
                Render::GL::g_pHyprOpenGL->renderBorder(box, color, {.round = roundScaled, .roundingPower = ROUND_PWR, .borderSize = BWIDTH});
            } else {
                Log::logger->log(Log::ERR, "[hyprexpo] invalid border color config: {}", effectiveSpec);
            }
        }
    };

    // Draw borders in order: hover (lowest), then current, then focus (highest priority)
    if (hoveredID != -1 && hoveredID != openedID && hoveredID != kbFocusID)
        drawBorderForID(hoveredID, std::string{*PBCOLHOV}, std::string{*PBGREHOV}, RND_HOV);
    drawBorderForID(openedID, std::string{*PBCOLCUR}, std::string{*PBGRCUR}, RND_CUR);
    if (kbFocusID != -1)
        drawBorderForID(kbFocusID, std::string{*PBCOLFOC}, std::string{*PBGREFOC}, RND_FOC);
    if (dragMoved && dragSourceID != -1) {
        const std::string sourceBorder = std::string{*PDRAGSOURCEBORDER}.empty() ? std::string{*PBCOLFOC} : std::string{*PDRAGSOURCEBORDER};
        const int         sourceWidth  = **PDRAGSOURCEBWIDTH >= 0 ? **PDRAGSOURCEBWIDTH : (int)**PBWIDTH;
        drawBorderForID(dragSourceID, sourceBorder, std::string{*PBGREFOC}, RND_FOC, sourceWidth);
    }

    if (dragWindow && isTileValid(dragSourceID)) {
        const auto windowBox = dragWindow->getWindowMainSurfaceBox();
        if (windowBox.w > 0 && windowBox.h > 0) {
            const CBox&  sourceBox = tileBoxes[dragSourceID];
            const double scaleX    = sourceBox.w / pMonitor->m_size.x;
            const double scaleY    = sourceBox.h / pMonitor->m_size.y;
            const double minW      = std::min(sourceBox.w, 24.0 * pMonitor->m_scale);
            const double minH      = std::min(sourceBox.h, 24.0 * pMonitor->m_scale);

            CBox proxy{
                lastMousePosLocal.x * pMonitor->m_scale - dragGrabOffset.x * scaleX,
                lastMousePosLocal.y * pMonitor->m_scale - dragGrabOffset.y * scaleY,
                std::clamp(windowBox.w * scaleX, minW, sourceBox.w),
                std::clamp(windowBox.h * scaleY, minH, sourceBox.h),
            };
            proxy.round();

            const int maxProxyRound = std::max(0, (int)std::floor(std::min(proxy.w, proxy.h) / 2.0));
            const int autoRound     = std::min(RND_FOC, maxProxyRound);
            const int round         = **PDRAGPROXYROUND >= 0 ? std::min(std::max(0, (int)std::lround((double)**PDRAGPROXYROUND * pMonitor->m_scale)), maxProxyRound) : autoRound;
            Render::GL::g_pHyprOpenGL->renderRect(proxy, CHyprColor{(uint64_t)(dragMoved ? **PDRAGPROXYACTCOL : **PDRAGPROXYCOL)}, {.round = round, .roundingPower = ROUND_PWR});

            const int   borderWidth   = **PDRAGPROXYBWIDTH >= 0 ? **PDRAGPROXYBWIDTH : std::max(2, (int)**PBWIDTH + 1);
            std::string effectiveSpec = std::string{*PDRAGPROXYBORDER}.empty() ? std::string{*PBCOLFOC} : std::string{*PDRAGPROXYBORDER};
            if (effectiveSpec.empty())
                effectiveSpec = std::string{*PBGREFOC};
            if (isGradientBorderSpec(effectiveSpec)) {
                const auto spec = parseGradientSpec(effectiveSpec);
                if (spec.valid) {
                    Config::CGradientValueData grad;
                    grad.m_colors.clear();
                    grad.m_colors.push_back(spec.c1);
                    grad.m_colors.push_back(spec.c2);
                    grad.m_angle = spec.angleDeg * (float)M_PI / 180.f;
                    grad.updateColorsOk();
                    if (borderWidth > 0)
                        Render::GL::g_pHyprOpenGL->renderBorder(proxy, grad, {.round = round, .roundingPower = ROUND_PWR, .borderSize = borderWidth});
                }
            } else if (!effectiveSpec.empty()) {
                Hyprexpo::SColorRGBA parsedColor;
                if (Hyprexpo::parseSolidColorSpec(effectiveSpec, parsedColor)) {
                    Config::CGradientValueData grad{CHyprColor{parsedColor.r, parsedColor.g, parsedColor.b, parsedColor.a}};
                    grad.updateColorsOk();
                    if (borderWidth > 0)
                        Render::GL::g_pHyprOpenGL->renderBorder(proxy, grad, {.round = round, .roundingPower = ROUND_PWR, .borderSize = borderWidth});
                } else
                    Log::logger->log(Log::ERR, "[hyprexpo] invalid drag_drop_proxy_border_color config: {}", effectiveSpec);
            }
        }
    }
}
