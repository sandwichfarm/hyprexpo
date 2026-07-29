#pragma once

#include <cstddef>

namespace HyprexpoConfig {
// PR #605 compatibility defaults kept separate from the richer sandwichfarm
// runtime options below. These values are registered for later integration and
// must not replace the newer config surface.
inline constexpr int         LEGACY_DYNAMIC_GRID_DEFAULT         = 0;
inline constexpr int         LEGACY_FILL_GAPS_DEFAULT            = 0;
inline constexpr int         LEGACY_MRU_SORT_DEFAULT             = 0;
inline constexpr unsigned    LEGACY_ACTIVE_HIGHLIGHT_COL_DEFAULT = 0xFF3584E4;
inline constexpr int         LEGACY_ACTIVE_HIGHLIGHT_BORDER_DEFAULT = 2;
inline constexpr unsigned    LEGACY_HOVER_HIGHLIGHT_COL_DEFAULT  = 0x80FFFFFF;
inline constexpr int         LEGACY_HOVER_HIGHLIGHT_BORDER_DEFAULT = 2;
inline constexpr const char* LEGACY_LABEL_POS_DEFAULT            = "top_right";
inline constexpr int         LEGACY_LABEL_SIZE_DEFAULT           = 36;
inline constexpr unsigned    LEGACY_LABEL_COL_DEFAULT            = 0xFFFFFFFF;
inline constexpr int         LEGACY_SHOW_WORKSPACE_NAMES_DEFAULT = 0;
inline constexpr int         LEGACY_ENABLE_KEYBOARD_NAV_DEFAULT  = 1;
inline constexpr int         LEGACY_ENABLE_DRAG_MOVE_DEFAULT     = 0;
inline constexpr int         LEGACY_ANIMATE_ENTRY_DEFAULT        = 0;
inline constexpr int         WALLPAPER_BG_DEFAULT                = 0;
inline constexpr std::size_t DYNAMIC_GRID_MAX_TILES              = 64;

inline constexpr int         COLUMNS_DEFAULT                 = 3;
inline constexpr int         COLUMNS_MIN                     = 1;
inline constexpr int         COLUMNS_MAX                     = 7;
inline constexpr int         GAPS_IN_DEFAULT                 = 5;
inline constexpr int         GAPS_OUT_DEFAULT                = 0;
inline constexpr unsigned    BG_COL_DEFAULT                  = 0xFF111111;
inline constexpr const char* WORKSPACE_METHOD_DEFAULT        = "center current";
inline constexpr int         SKIP_EMPTY_DEFAULT              = 0;
inline constexpr int         MAX_WORKSPACE_DEFAULT           = 0;
inline constexpr int         SHOW_WORKSPACE_NUMBERS_DEFAULT  = 0;
inline constexpr unsigned    WORKSPACE_NUMBER_COLOR_DEFAULT  = 0xFFFFFFFF;
inline constexpr int         GESTURE_DISTANCE_DEFAULT        = 200;
inline constexpr const char* CANCEL_KEY_DEFAULT              = "escape";
inline constexpr int         SHOW_CURSOR_DEFAULT             = 1;
inline constexpr int         SHOW_PINNED_WINDOWS_DEFAULT     = 0;
inline constexpr int         DRAG_DROP_ENABLE_DEFAULT        = 1;
inline constexpr int         KEYNAV_ENABLE_DEFAULT           = 1;
inline constexpr int         KEYNAV_WRAP_H_DEFAULT           = 1;
inline constexpr int         KEYNAV_WRAP_V_DEFAULT           = 1;
inline constexpr int         KEYNAV_READING_ORDER_DEFAULT    = 0;
inline constexpr int         BORDER_WIDTH_DEFAULT            = 2;
inline constexpr const char* BORDER_COLOR_DEFAULT            = "";
inline constexpr const char* BORDER_COLOR_CURRENT_DEFAULT    = "rgb(66ccff)";
inline constexpr const char* BORDER_COLOR_FOCUS_DEFAULT      = "rgb(ffcc66)";
inline constexpr const char* BORDER_COLOR_HOVER_DEFAULT      = "rgb(aabbcc)";
inline constexpr const char* BORDER_STYLE_DEFAULT            = "simple";
inline constexpr const char* BORDER_GRAD_CURRENT_DEFAULT     = "";
inline constexpr const char* BORDER_GRAD_FOCUS_DEFAULT       = "";
inline constexpr const char* BORDER_GRAD_HOVER_DEFAULT       = "";
inline constexpr unsigned    DRAG_DROP_PROXY_COLOR_DEFAULT        = 0x24EDB342;
inline constexpr unsigned    DRAG_DROP_PROXY_ACTIVE_COLOR_DEFAULT = 0x3DEDB342;
inline constexpr const char* DRAG_DROP_PROXY_BORDER_COLOR_DEFAULT = "";
inline constexpr int         DRAG_DROP_PROXY_BORDER_WIDTH_DEFAULT = -1;
inline constexpr int         DRAG_DROP_PROXY_ROUNDING_DEFAULT     = -1;
inline constexpr const char* DRAG_DROP_SOURCE_BORDER_COLOR_DEFAULT = "";
inline constexpr int         DRAG_DROP_SOURCE_BORDER_WIDTH_DEFAULT = -1;
inline constexpr int         LABEL_ENABLE_DEFAULT            = 1;
inline constexpr unsigned    LABEL_COLOR_DEFAULT_LEGACY      = 0xFFFFFFFF;
inline constexpr int         LABEL_FONT_SIZE_DEFAULT         = 16;
inline constexpr const char* LABEL_TEXT_MODE_DEFAULT         = "token";
inline constexpr const char* LABEL_TOKEN_MAP_DEFAULT         = "";
inline constexpr const char* LABEL_POSITION_DEFAULT          = "center";
inline constexpr int         LABEL_OFFSET_X_DEFAULT          = 0;
inline constexpr int         LABEL_OFFSET_Y_DEFAULT          = 0;
inline constexpr const char* LABEL_SHOW_DEFAULT              = "always";
inline constexpr unsigned    LABEL_COLOR_DEFAULT             = 0xFFFFFFFF;
inline constexpr unsigned    LABEL_COLOR_HOVER_DEFAULT       = 0xFFEEEEEE;
inline constexpr unsigned    LABEL_COLOR_FOCUS_DEFAULT       = 0xFFFFCC66;
inline constexpr unsigned    LABEL_COLOR_CURRENT_DEFAULT     = 0xFF66CCFF;
inline constexpr float       LABEL_SCALE_HOVER_DEFAULT       = 1.0F;
inline constexpr float       LABEL_SCALE_FOCUS_DEFAULT       = 1.0F;
inline constexpr int         LABEL_BG_ENABLE_DEFAULT         = 1;
inline constexpr unsigned    LABEL_BG_COLOR_DEFAULT          = 0x88000000;
inline constexpr int         LABEL_BG_ROUNDING_DEFAULT       = 8;
inline constexpr const char* LABEL_BG_SHAPE_DEFAULT          = "circle";
inline constexpr int         LABEL_PADDING_DEFAULT           = 8;
inline constexpr const char* LABEL_FONT_FAMILY_DEFAULT       = "sans";
inline constexpr int         LABEL_FONT_BOLD_DEFAULT         = 0;
inline constexpr int         LABEL_FONT_ITALIC_DEFAULT       = 0;
inline constexpr int         LABEL_TEXT_UNDERLINE_DEFAULT    = 0;
inline constexpr int         LABEL_TEXT_STRIKETHROUGH_DEFAULT = 0;
inline constexpr int         LABEL_PIXEL_SNAP_DEFAULT        = 1;
inline constexpr int         LABEL_CENTER_ADJUST_X_DEFAULT   = 0;
inline constexpr int         LABEL_CENTER_ADJUST_Y_DEFAULT   = 0;
inline constexpr int         TILE_ROUNDING_DEFAULT           = 0;
inline constexpr float       TILE_ROUNDING_POWER_DEFAULT     = 2.0F;
inline constexpr int         TILE_ROUNDING_FOCUS_DEFAULT     = -1;
inline constexpr int         TILE_ROUNDING_CURRENT_DEFAULT   = -1;
inline constexpr int         TILE_ROUNDING_HOVER_DEFAULT     = -1;
inline constexpr int         SELECTION_LABEL_ENABLE_DEFAULT  = 0;
inline constexpr const char* SELECTION_LABEL_TOKEN_MAP_DEFAULT = "a,s,d,f,g,q,w,e,r,t,z,x,c,v,b";
inline constexpr const char* SELECTION_LABEL_POSITION_DEFAULT  = "top-right";
inline constexpr int         SELECTION_LABEL_OFFSET_X_DEFAULT  = 6;
inline constexpr int         SELECTION_LABEL_OFFSET_Y_DEFAULT  = 6;
inline constexpr unsigned    SELECTION_LABEL_COLOR_DEFAULT     = 0xFFFFCC66;
}
