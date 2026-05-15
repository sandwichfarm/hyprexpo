#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/input/trackpad/GestureTypes.hpp>
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>

#include <hyprutils/string/ConstVarList.hpp>
#include <lua.hpp>
using namespace Hyprutils::String;

#include <cctype>
#include <optional>

#include "globals.hpp"
#include "overview.hpp"
#include "ExpoGesture.hpp"
#include <hyprland/src/event/EventBus.hpp>

// Methods
inline CFunctionHook* g_pRenderWorkspaceHook = nullptr;
inline CFunctionHook* g_pAddDamageHookA      = nullptr;
inline CFunctionHook* g_pAddDamageHookB      = nullptr;
typedef void (*origRenderWorkspace)(void*, PHLMONITOR, PHLWORKSPACE, timespec*, const CBox&);
typedef void (*origAddDamageA)(void*, const CBox&);
typedef void (*origAddDamageB)(void*, const pixman_region32_t*);

static bool g_unloading = false;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static bool renderingOverview = false;

// forward declarations for new dispatchers
static SDispatchResult onExpoDispatcher(std::string arg);
static SDispatchResult onKbFocusDispatcher(std::string arg);
static SDispatchResult onKbConfirmDispatcher(std::string arg);
static SDispatchResult onKbSelectNumberDispatcher(std::string arg);
static SDispatchResult onKbSelectTokenDispatcher(std::string arg);
static SDispatchResult onKbSelectIndexDispatcher(std::string arg);

static int luaDispatchResult(lua_State* L, const char* name, const SDispatchResult& result) {
    if (result.success)
        return 0;

    return luaL_error(L, "%s: %s", name, result.error.empty() ? "dispatcher failed" : result.error.c_str());
}

static std::string luaStringArg(lua_State* L, int index, const char* name, const char* defaultValue = "") {
    if (lua_gettop(L) < index || lua_isnil(L, index))
        return defaultValue;

    if (lua_isnumber(L, index))
        return std::to_string(lua_tointeger(L, index));

    if (lua_isstring(L, index))
        return lua_tostring(L, index);

    luaL_error(L, "%s: argument %d must be a string or integer", name, index);
    return defaultValue;
}

static int luaExpo(lua_State* L) {
    return luaDispatchResult(L, "hyprexpo.expo", onExpoDispatcher(luaStringArg(L, 1, "hyprexpo.expo", "toggle")));
}

static int luaKbFocus(lua_State* L) {
    return luaDispatchResult(L, "hyprexpo.kb_focus", onKbFocusDispatcher(luaStringArg(L, 1, "hyprexpo.kb_focus")));
}

static int luaKbConfirm(lua_State* L) {
    return luaDispatchResult(L, "hyprexpo.kb_confirm", onKbConfirmDispatcher(""));
}

static int luaKbSelectNumber(lua_State* L) {
    return luaDispatchResult(L, "hyprexpo.kb_selectn", onKbSelectNumberDispatcher(luaStringArg(L, 1, "hyprexpo.kb_selectn")));
}

static int luaKbSelectToken(lua_State* L) {
    return luaDispatchResult(L, "hyprexpo.kb_select", onKbSelectTokenDispatcher(luaStringArg(L, 1, "hyprexpo.kb_select")));
}

static int luaKbSelectIndex(lua_State* L) {
    return luaDispatchResult(L, "hyprexpo.kb_selecti", onKbSelectIndexDispatcher(luaStringArg(L, 1, "hyprexpo.kb_selecti")));
}

//
static void hkRenderWorkspace(void* thisptr, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, timespec* now, const CBox& geometry) {
    if (!g_pOverview || renderingOverview || g_pOverview->blockOverviewRendering || g_pOverview->pMonitor != pMonitor)
        ((origRenderWorkspace)(g_pRenderWorkspaceHook->m_original))(thisptr, pMonitor, pWorkspace, now, geometry);
    else
        g_pOverview->render();
}

static void hkAddDamageA(void* thisptr, const CBox& box) {
    const auto PMONITOR = (CMonitor*)thisptr;

    if (!g_pOverview || g_pOverview->pMonitor != PMONITOR->m_self || g_pOverview->blockDamageReporting) {
        ((origAddDamageA)g_pAddDamageHookA->m_original)(thisptr, box);
        return;
    }

    g_pOverview->onDamageReported();
}

static void hkAddDamageB(void* thisptr, const pixman_region32_t* rg) {
    const auto PMONITOR = (CMonitor*)thisptr;

    if (!g_pOverview || g_pOverview->pMonitor != PMONITOR->m_self || g_pOverview->blockDamageReporting) {
        ((origAddDamageB)g_pAddDamageHookB->m_original)(thisptr, rg);
        return;
    }

    g_pOverview->onDamageReported();
}

static SDispatchResult onExpoDispatcher(std::string arg) {

    if (g_pOverview && g_pOverview->m_isSwiping)
        return {.success = false, .error = "already swiping"};

    if (arg == "select") {
        if (g_pOverview) {
            g_pOverview->selectHoveredWorkspace();
            g_pOverview->close();
        }
        return {};
    }
    if (arg == "toggle") {
        if (g_pOverview)
            g_pOverview->close();
        else {
            renderingOverview = true;
            g_pOverview       = std::make_unique<COverview>(g_pCompositor->getMonitorFromCursor()->m_activeWorkspace);
            renderingOverview = false;
        }
        return {};
    }

    if (arg == "off" || arg == "close" || arg == "disable") {
        if (g_pOverview)
            g_pOverview->close();
        return {};
    }

    if (g_pOverview)
        return {};

    renderingOverview = true;
    g_pOverview       = std::make_unique<COverview>(g_pCompositor->getMonitorFromCursor()->m_activeWorkspace);
    renderingOverview = false;
    return {};
}

static void failNotif(const std::string& reason) {
    HyprlandAPI::addNotification(PHANDLE, "[hyprexpo] Failure in initialization: " + reason, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
}

static Hyprlang::CParseResult workspaceMethodKeyword(const char* LHS, const char* RHS) {
    Hyprlang::CParseResult result;

    if (g_unloading)
        return result;

    // Parse format - accepts both:
    //   2 args: "method workspace" (global default)
    //   3 args: "MONITOR_NAME method workspace" (per-monitor)
    CConstVarList data(RHS);

    if (data.size() == 2) {
        // Global format - not really needed since plugin config does this, but accept it
        const std::string methodType = std::string{data[0]};
        const std::string workspace = std::string{data[1]};

        if (methodType != "center" && methodType != "first") {
            result.setError(std::format("Invalid method type '{}', expected 'center' or 'first'", methodType).c_str());
            return result;
        }

        // Don't store - let plugin config handle global default
        // Just return success so it doesn't error
        return result;

    } else if (data.size() == 3) {
        // Per-monitor format
        const std::string monitorName = std::string{data[0]};
        const std::string methodType = std::string{data[1]};
        const std::string workspace = std::string{data[2]};

        if (methodType != "center" && methodType != "first") {
            result.setError(std::format("Invalid method type '{}', expected 'center' or 'first'", methodType).c_str());
            return result;
        }

        // Store in global map
        g_monitorWorkspaceMethods[monitorName] = methodType + " " + workspace;
        return result;

    } else {
        result.setError("hyprexpo_workspace_method requires format: <center|first> <workspace> OR MONITOR_NAME <center|first> <workspace>");
        return result;
    }
}

static Hyprlang::CParseResult expoGestureKeyword(const char* LHS, const char* RHS) {
    Hyprlang::CParseResult    result;

    if (g_unloading)
        return result;

    CConstVarList             data(RHS);

    size_t                    fingerCount = 0;
    eTrackpadGestureDirection direction   = TRACKPAD_GESTURE_DIR_NONE;

    try {
        fingerCount = std::stoul(std::string{data[0]});
    } catch (...) {
        result.setError(std::format("Invalid value {} for finger count", data[0]).c_str());
        return result;
    }

    if (fingerCount <= 1 || fingerCount >= 10) {
        result.setError(std::format("Invalid value {} for finger count", data[0]).c_str());
        return result;
    }

    direction = g_pTrackpadGestures->dirForString(data[1]);

    if (direction == TRACKPAD_GESTURE_DIR_NONE) {
        result.setError(std::format("Invalid direction: {}", data[1]).c_str());
        return result;
    }

    int      startDataIdx = 2;
    uint32_t modMask      = 0;
    float    deltaScale   = 1.F;

    while (true) {

        if (data[startDataIdx].starts_with("mod:")) {
            modMask = g_pKeybindManager->stringToModMask(std::string{data[startDataIdx].substr(4)});
            startDataIdx++;
            continue;
        } else if (data[startDataIdx].starts_with("scale:")) {
            try {
                deltaScale = std::clamp(std::stof(std::string{data[startDataIdx].substr(6)}), 0.1F, 10.F);
                startDataIdx++;
                continue;
            } catch (...) {
                result.setError(std::format("Invalid delta scale: {}", std::string{data[startDataIdx].substr(6)}).c_str());
                return result;
            }
        }

        break;
    }

    std::expected<void, std::string> resultFromGesture;

    if (data[startDataIdx] == "expo")
        resultFromGesture = g_pTrackpadGestures->addGesture(makeUnique<CExpoGesture>(), fingerCount, direction, modMask, deltaScale, false);
    else if (data[startDataIdx] == "unset")
        resultFromGesture = g_pTrackpadGestures->removeGesture(fingerCount, direction, modMask, deltaScale, false);
    else {
        result.setError(std::format("Invalid gesture: {}", data[startDataIdx]).c_str());
        return result;
    }

    if (!resultFromGesture) {
        result.setError(resultFromGesture.error().c_str());
        return result;
    }

    return result;
}

static void addConfigValue(SP<Config::Values::IValue> value) {
    HyprlandAPI::addConfigValueV2(PHANDLE, value);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != __hyprland_api_get_client_hash()) {
        failNotif("Version mismatch (headers ver is not equal to running hyprland ver)");
        throw std::runtime_error("[he] Version mismatch");
    }

    auto FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "renderWorkspace");
    if (FNS.empty()) {
        failNotif("no fns for hook renderWorkspace");
        throw std::runtime_error("[he] No fns for hook renderWorkspace");
    }

    g_pRenderWorkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkRenderWorkspace);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "addDamageEPK15pixman_region32");
    if (FNS.empty()) {
        failNotif("no fns for hook addDamageEPK15pixman_region32");
        throw std::runtime_error("[he] No fns for hook addDamageEPK15pixman_region32");
    }

    g_pAddDamageHookB = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageB);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "_ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
    if (FNS.empty()) {
        failNotif("no fns for hook _ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
        throw std::runtime_error("[he] No fns for hook _ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
    }

    g_pAddDamageHookA = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageA);

    bool success = g_pRenderWorkspaceHook->hook();
    success      = success && g_pAddDamageHookA->hook();
    success      = success && g_pAddDamageHookB->hook();

    if (!success) {
        failNotif("Failed initializing hooks");
        throw std::runtime_error("[he] Failed initializing hooks");
    }

    static auto P = Event::bus()->m_events.render.pre.listen([](PHLMONITOR pMonitor) {
        if (!g_pOverview)
            return;
        g_pOverview->onPreRender();
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:expo", ::onExpoDispatcher);

    // keyboard navigation dispatchers
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:kb_focus", ::onKbFocusDispatcher);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:kb_confirm", ::onKbConfirmDispatcher);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:kb_selectn", ::onKbSelectNumberDispatcher);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:kb_select", ::onKbSelectTokenDispatcher);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:kb_selecti", ::onKbSelectIndexDispatcher);

    HyprlandAPI::addLuaFunction(PHANDLE, "hyprexpo", "expo", ::luaExpo);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprexpo", "kb_focus", ::luaKbFocus);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprexpo", "kb_confirm", ::luaKbConfirm);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprexpo", "kb_selectn", ::luaKbSelectNumber);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprexpo", "kb_select", ::luaKbSelectToken);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprexpo", "kb_selecti", ::luaKbSelectIndex);

    HyprlandAPI::addConfigKeyword(PHANDLE, "hyprexpo_gesture", ::expoGestureKeyword, {});
    HyprlandAPI::addConfigKeyword(PHANDLE, "hyprexpo_workspace_method", ::workspaceMethodKeyword, {});

    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:columns", "columns", 3));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:gaps_in", "inner gaps", 5));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:bg_col", "background color", 0xFF111111));
    // Supports both global and per-monitor formats:
    // Global: "center current" or "first 1"
    // Per-monitor with comma delimiter: "DP-1 first 1, HDMI-1 center current"
    // Mixed: "DP-1 first 1, center current" (DP-1 uses first 1, others use center current)
    // Note: hyprexpo_workspace_method keyword takes priority (backwards compatibility)
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:workspace_method", "workspace method", "center current"));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:skip_empty", "skip empty workspaces", 0));

    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:gesture_distance", "gesture distance", 200));

    // keyboard navigation + styling
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:keynav_enable", "key navigation enable", 1));
    // Border configuration - supports both solid colors and gradients
    // Solid: rgb(rrggbb) or 0xAARRGGBB
    // Gradient: rgba(rrggbbaa) rgba(rrggbbaa) 45deg
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:border_width", "border width", 2));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_color", "border color", ""));           // default border (unused tiles)
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_color_current", "current border color", "rgb(66ccff)"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_color_focus", "focus border color", "rgb(ffcc66)"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_color_hover", "hover border color", "rgb(aabbcc)"));
    // Deprecated but supported for backwards compatibility
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_style", "border style", "simple"));     // ignored, auto-detected from format
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_enable", "label enable", 1));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_color", "label color", 0xFFFFFFFF));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_font_size", "label font size", 16));
    // label_text_mode: token (default) | id | index
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_text_mode", "label text mode", "token"));
    // Optional override map for up to 50 tokens, comma-separated. Empty entries allowed.
    // Example: "1,2,3,4,5,6,7,8,9,0,!,@,#,$,%,^,&,*,(,),a,..."
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_token_map", "label token map", ""));

    // tile rounding (rounded corners for workspace previews)
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:tile_rounding", "tile rounding", 0));
    addConfigValue(makeShared<Config::Values::CFloatValue>("plugin:hyprexpo:tile_rounding_power", "tile rounding power", 2.0F));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:tile_rounding_focus", "focus tile rounding", -1));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:tile_rounding_current", "current tile rounding", -1));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:tile_rounding_hover", "hover tile rounding", -1));

    // (shadows moved to feature/shadows branch)
    // defaults: center/middle within the label container
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_position", "label position", "center"));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_offset_x", "label offset x", 0));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_offset_y", "label offset y", 0));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_show", "label show", "always"));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_color_default", "default label color", 0xFFFFFFFF));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_color_hover", "hover label color", 0xFFEEEEEE));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_color_focus", "focus label color", 0xFFFFCC66));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_color_current", "current label color", 0xFF66CCFF));
    addConfigValue(makeShared<Config::Values::CFloatValue>("plugin:hyprexpo:label_scale_hover", "hover label scale", 1.0F));
    addConfigValue(makeShared<Config::Values::CFloatValue>("plugin:hyprexpo:label_scale_focus", "focus label scale", 1.0F));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_bg_enable", "label background enable", 1));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_bg_color", "label background color", 0x88000000));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_bg_rounding", "label background rounding", 8));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_bg_shape", "label background shape", "circle"));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_padding", "label padding", 8));
    // label font styling and pixel snapping
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_font_family", "label font family", "sans"));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_font_bold", "label font bold", 0));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_font_italic", "label font italic", 0));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_text_underline", "label text underline", 0));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_text_strikethrough", "label text strikethrough", 0));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_pixel_snap", "label pixel snap", 1));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_center_adjust_x", "label center adjust x", 0));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_center_adjust_y", "label center adjust y", 0));
    // gaps
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:gaps_out", "outer gaps", 0));
    // Deprecated: use border_color_* instead (supports both solid and gradient)
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_grad_current", "current border gradient", ""));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_grad_focus", "focus border gradient", ""));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_grad_hover", "hover border gradient", ""));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:keynav_wrap_h", "key navigation horizontal wrap", 1));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:keynav_wrap_v", "key navigation vertical wrap", 1));
    // default off: spatial moves by default
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:keynav_reading_order", "key navigation reading order", 0));

    HyprlandAPI::reloadConfig();

    return {"hyprexpo-plus", "hyprexpo+ with keyboard selection, labels, and borders", "sandwich", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pHyprRenderer->m_renderPass.removeAllOfType("COverviewPassElement");

    g_unloading = true;

    Config::mgr()->reload(); // we need to reload now to clear all the gestures
}

//
// New dispatchers for keyboard navigation
//

static SDispatchResult onKbFocusDispatcher(std::string arg) {
    if (!g_pOverview)
        return {};

    if (arg == "left" || arg == "right" || arg == "up" || arg == "down") {
        g_pOverview->onKbMoveFocus(arg);
        return {};
    }

    return {.success = false, .error = "invalid arg. expected left|right|up|down"};
}

static SDispatchResult onKbConfirmDispatcher(std::string arg) {
    if (!g_pOverview)
        return {};

    g_pOverview->onKbConfirm();
    return {};
}

static SDispatchResult onKbSelectNumberDispatcher(std::string arg) {
    if (!g_pOverview)
        return {};

    // trim spaces
    while (!arg.empty() && std::isspace(arg.front()))
        arg.erase(arg.begin());
    while (!arg.empty() && std::isspace(arg.back()))
        arg.pop_back();

    if (arg.empty())
        return {.success = false, .error = "missing number"};

    int num = -1;
    try {
        num = std::stoi(arg);
    } catch (...) {
        return {.success = false, .error = "invalid number"};
    }

    g_pOverview->onKbSelectNumber(num);
    return {};
}

static std::optional<int> tokenToIndex(const std::string& s) {
    if (s.size() != 1)
        return std::nullopt;
    const char c = s[0];
    if (c >= '1' && c <= '9')
        return (c - '1');
    if (c == '0')
        return 9;
    if (c >= 'a' && c <= 'z')
        return 10 + (c - 'a');
    if (c >= 'A' && c <= 'Z')
        return 10 + (c - 'A');
    return std::nullopt;
}

static SDispatchResult onKbSelectTokenDispatcher(std::string arg) {
    if (!g_pOverview)
        return {};
    while (!arg.empty() && std::isspace(arg.front())) arg.erase(arg.begin());
    while (!arg.empty() && std::isspace(arg.back())) arg.pop_back();
    const auto idx = tokenToIndex(arg);
    if (!idx)
        return {.success = false, .error = "invalid token (expected 1..9, 0, a..z)"};
    g_pOverview->onKbSelectToken(*idx);
    return {};
}

static SDispatchResult onKbSelectIndexDispatcher(std::string arg) {
    if (!g_pOverview)
        return {};
    // trim
    while (!arg.empty() && std::isspace(arg.front())) arg.erase(arg.begin());
    while (!arg.empty() && std::isspace(arg.back())) arg.pop_back();
    int idx = -1;
    try { idx = std::stoi(arg); } catch (...) { idx = -1; }
    if (idx <= 0)
        return {.success = false, .error = "invalid index (expected >= 1)"};
    // convert to 0-based visible index
    g_pOverview->onKbSelectToken(idx - 1);
    return {};
}
