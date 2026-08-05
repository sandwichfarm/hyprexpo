#define WLR_USE_UNSTABLE

#include "PluginConfig.hpp"

#include "Dispatchers.hpp"
#include "globals.hpp"
#include "HyprexpoConfig.hpp"
#include <hyprland/src/config/values/types/ColorValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/managers/input/trackpad/GestureTypes.hpp>
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#include <expected>
#include <string>

static void addConfigValue(SP<Config::Values::IValue> value) {
    HyprlandAPI::addConfigValueV2(PHANDLE, value);
}

// Hyprlang skips plugin validators; runtime sync validates again.
static std::expected<void, std::string> validateGestureDirection(const Config::STRING& value) {
    if (!g_pTrackpadGestures || g_pTrackpadGestures->dirForString(value) != TRACKPAD_GESTURE_DIR_NONE)
        return {};

    return std::unexpected("invalid direction '" + value + "'");
}

void registerHyprexpoConfigValues() {
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:dynamic_grid", "legacy dynamic grid toggle", HyprexpoConfig::LEGACY_DYNAMIC_GRID_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:fill_gaps", "legacy gap filling toggle", HyprexpoConfig::LEGACY_FILL_GAPS_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:mru_sort", "legacy mru sorting toggle", HyprexpoConfig::LEGACY_MRU_SORT_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:active_highlight_col", "legacy active tile highlight color", HyprexpoConfig::LEGACY_ACTIVE_HIGHLIGHT_COL_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:active_highlight_border", "legacy active tile highlight border", HyprexpoConfig::LEGACY_ACTIVE_HIGHLIGHT_BORDER_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:hover_highlight_col", "legacy hovered tile highlight color", HyprexpoConfig::LEGACY_HOVER_HIGHLIGHT_COL_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:hover_highlight_border", "legacy hovered tile highlight border", HyprexpoConfig::LEGACY_HOVER_HIGHLIGHT_BORDER_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_pos", "legacy label anchor", HyprexpoConfig::LEGACY_LABEL_POS_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_size", "legacy label font size", HyprexpoConfig::LEGACY_LABEL_SIZE_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:label_col", "legacy label color", HyprexpoConfig::LEGACY_LABEL_COL_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:show_workspace_names", "legacy workspace-name label toggle", HyprexpoConfig::LEGACY_SHOW_WORKSPACE_NAMES_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:enable_keyboard_nav", "legacy keyboard navigation toggle", HyprexpoConfig::LEGACY_ENABLE_KEYBOARD_NAV_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:enable_drag_move", "legacy drag-move toggle", HyprexpoConfig::LEGACY_ENABLE_DRAG_MOVE_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:animate_entry", "legacy entry animation toggle", HyprexpoConfig::LEGACY_ANIMATE_ENTRY_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:wallpaper_bg", "draw the monitor wallpaper behind overview tiles", HyprexpoConfig::WALLPAPER_BG_DEFAULT));

    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:columns", "columns", HyprexpoConfig::COLUMNS_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:gaps_in", "inner gaps", HyprexpoConfig::GAPS_IN_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:bg_col", "background color", HyprexpoConfig::BG_COL_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:workspace_method", "workspace method", HyprexpoConfig::WORKSPACE_METHOD_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:skip_empty", "skip empty workspaces", HyprexpoConfig::SKIP_EMPTY_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:max_workspace", "maximum sequential workspace", HyprexpoConfig::MAX_WORKSPACE_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:show_workspace_numbers", "force workspace ID labels", HyprexpoConfig::SHOW_WORKSPACE_NUMBERS_DEFAULT));

    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:gesture_distance", "gesture distance", HyprexpoConfig::GESTURE_DISTANCE_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:gesture_fingers", "fingers for the swipe gesture (0 disables)", HyprexpoConfig::GESTURE_FINGERS_DEFAULT,
                                                         Config::Values::SIntValueOptions{.min = 0, .max = 9}));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:gesture_direction", "swipe direction for the gesture", HyprexpoConfig::GESTURE_DIRECTION_DEFAULT,
                                                            Config::Values::SStringValueOptions{.validator = validateGestureDirection}));
    addConfigValue(createCancelKeyConfig());
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:show_cursor", "show cursor during overview", HyprexpoConfig::SHOW_CURSOR_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:show_pinned_windows", "show pinned windows in previews", HyprexpoConfig::SHOW_PINNED_WINDOWS_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:drag_drop_enable", "enable drag/drop window movement", HyprexpoConfig::DRAG_DROP_ENABLE_DEFAULT));

    // keyboard navigation + styling
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:keynav_enable", "key navigation enable", HyprexpoConfig::KEYNAV_ENABLE_DEFAULT));
    addConfigValue(createNumberKeyModeConfig());
    // Border configuration - supports both solid colors and gradients
    // Solid: rgb(rrggbb) or 0xAARRGGBB
    // Gradient: rgba(rrggbbaa) rgba(rrggbbaa) 45deg
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:border_width", "border width", HyprexpoConfig::BORDER_WIDTH_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_color", "border color", HyprexpoConfig::BORDER_COLOR_DEFAULT));           // default border (unused tiles)
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_color_current", "current border color", HyprexpoConfig::BORDER_COLOR_CURRENT_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_color_focus", "focus border color", HyprexpoConfig::BORDER_COLOR_FOCUS_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_color_hover", "hover border color", HyprexpoConfig::BORDER_COLOR_HOVER_DEFAULT));
    // Deprecated but supported for backwards compatibility
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_style", "border style", HyprexpoConfig::BORDER_STYLE_DEFAULT));     // ignored, auto-detected from format

    // Drag/drop window movement styling. Empty border specs inherit the focused tile border.
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:drag_drop_proxy_color", "drag/drop proxy color", HyprexpoConfig::DRAG_DROP_PROXY_COLOR_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:drag_drop_proxy_active_color", "drag/drop active proxy color", HyprexpoConfig::DRAG_DROP_PROXY_ACTIVE_COLOR_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:drag_drop_proxy_border_color", "drag/drop proxy border color", HyprexpoConfig::DRAG_DROP_PROXY_BORDER_COLOR_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:drag_drop_proxy_border_width", "drag/drop proxy border width", HyprexpoConfig::DRAG_DROP_PROXY_BORDER_WIDTH_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:drag_drop_proxy_rounding", "drag/drop proxy rounding", HyprexpoConfig::DRAG_DROP_PROXY_ROUNDING_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:drag_drop_source_border_color", "drag/drop source border color", HyprexpoConfig::DRAG_DROP_SOURCE_BORDER_COLOR_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:drag_drop_source_border_width", "drag/drop source border width", HyprexpoConfig::DRAG_DROP_SOURCE_BORDER_WIDTH_DEFAULT));

    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_enable", "label enable", HyprexpoConfig::LABEL_ENABLE_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:label_color", "label color", HyprexpoConfig::LABEL_COLOR_DEFAULT_LEGACY));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_font_size", "label font size", HyprexpoConfig::LABEL_FONT_SIZE_DEFAULT));
    // label_text_mode: token (default) | id | index
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_text_mode", "label text mode", HyprexpoConfig::LABEL_TEXT_MODE_DEFAULT));
    // Optional override map for up to 50 tokens, comma-separated. Empty entries allowed.
    // Example: "1,2,3,4,5,6,7,8,9,0,!,@,#,$,%,^,&,*,(,),a,..."
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_token_map", "label token map", HyprexpoConfig::LABEL_TOKEN_MAP_DEFAULT));

    // tile rounding (rounded corners for workspace previews)
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:tile_rounding", "tile rounding", HyprexpoConfig::TILE_ROUNDING_DEFAULT));
    addConfigValue(makeShared<Config::Values::CFloatValue>("plugin:hyprexpo:tile_rounding_power", "tile rounding power", HyprexpoConfig::TILE_ROUNDING_POWER_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:tile_rounding_focus", "focus tile rounding", HyprexpoConfig::TILE_ROUNDING_FOCUS_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:tile_rounding_current", "current tile rounding", HyprexpoConfig::TILE_ROUNDING_CURRENT_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:tile_rounding_hover", "hover tile rounding", HyprexpoConfig::TILE_ROUNDING_HOVER_DEFAULT));

    // (shadows moved to feature/shadows branch)
    // defaults: center/middle within the label container
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_position", "label position", HyprexpoConfig::LABEL_POSITION_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_offset_x", "label offset x", HyprexpoConfig::LABEL_OFFSET_X_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_offset_y", "label offset y", HyprexpoConfig::LABEL_OFFSET_Y_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:selection_label_enable", "selection label enable", HyprexpoConfig::SELECTION_LABEL_ENABLE_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:selection_label_token_map", "selection label token map", HyprexpoConfig::SELECTION_LABEL_TOKEN_MAP_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:selection_label_position", "selection label position", HyprexpoConfig::SELECTION_LABEL_POSITION_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:selection_label_offset_x", "selection label offset x", HyprexpoConfig::SELECTION_LABEL_OFFSET_X_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:selection_label_offset_y", "selection label offset y", HyprexpoConfig::SELECTION_LABEL_OFFSET_Y_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:selection_label_color", "selection label color", HyprexpoConfig::SELECTION_LABEL_COLOR_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_show", "label show", HyprexpoConfig::LABEL_SHOW_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:label_color_default", "default label color", HyprexpoConfig::LABEL_COLOR_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:label_color_hover", "hover label color", HyprexpoConfig::LABEL_COLOR_HOVER_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:label_color_focus", "focus label color", HyprexpoConfig::LABEL_COLOR_FOCUS_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:label_color_current", "current label color", HyprexpoConfig::LABEL_COLOR_CURRENT_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:workspace_number_color", "workspace number label color", HyprexpoConfig::WORKSPACE_NUMBER_COLOR_DEFAULT));
    addConfigValue(makeShared<Config::Values::CFloatValue>("plugin:hyprexpo:label_scale_hover", "hover label scale", HyprexpoConfig::LABEL_SCALE_HOVER_DEFAULT));
    addConfigValue(makeShared<Config::Values::CFloatValue>("plugin:hyprexpo:label_scale_focus", "focus label scale", HyprexpoConfig::LABEL_SCALE_FOCUS_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_bg_enable", "label background enable", HyprexpoConfig::LABEL_BG_ENABLE_DEFAULT));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:label_bg_color", "label background color", HyprexpoConfig::LABEL_BG_COLOR_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_bg_rounding", "label background rounding", HyprexpoConfig::LABEL_BG_ROUNDING_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_bg_shape", "label background shape", HyprexpoConfig::LABEL_BG_SHAPE_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_padding", "label padding", HyprexpoConfig::LABEL_PADDING_DEFAULT));
    // label font styling and pixel snapping
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:label_font_family", "label font family", HyprexpoConfig::LABEL_FONT_FAMILY_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_font_bold", "label font bold", HyprexpoConfig::LABEL_FONT_BOLD_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_font_italic", "label font italic", HyprexpoConfig::LABEL_FONT_ITALIC_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_text_underline", "label text underline", HyprexpoConfig::LABEL_TEXT_UNDERLINE_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_text_strikethrough", "label text strikethrough", HyprexpoConfig::LABEL_TEXT_STRIKETHROUGH_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_pixel_snap", "label pixel snap", HyprexpoConfig::LABEL_PIXEL_SNAP_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_center_adjust_x", "label center adjust x", HyprexpoConfig::LABEL_CENTER_ADJUST_X_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:label_center_adjust_y", "label center adjust y", HyprexpoConfig::LABEL_CENTER_ADJUST_Y_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:gaps_out", "outer gaps", HyprexpoConfig::GAPS_OUT_DEFAULT));
    // Deprecated: use border_color_* instead (supports both solid and gradient)
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_grad_current", "current border gradient", HyprexpoConfig::BORDER_GRAD_CURRENT_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_grad_focus", "focus border gradient", HyprexpoConfig::BORDER_GRAD_FOCUS_DEFAULT));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:border_grad_hover", "hover border gradient", HyprexpoConfig::BORDER_GRAD_HOVER_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:keynav_wrap_h", "key navigation horizontal wrap", HyprexpoConfig::KEYNAV_WRAP_H_DEFAULT));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:keynav_wrap_v", "key navigation vertical wrap", HyprexpoConfig::KEYNAV_WRAP_V_DEFAULT));
    // default off: spatial moves by default
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:keynav_reading_order", "key navigation reading order", HyprexpoConfig::KEYNAV_READING_ORDER_DEFAULT));
}
