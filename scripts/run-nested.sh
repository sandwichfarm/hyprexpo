#!/usr/bin/env bash
set -euo pipefail

# Launch a nested Hyprland session that loads the local hyprexpo.so,
# so you can test changes without restarting your main session.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/hyprexpo"
SO="${HYPREXPO_DEV_SO:-$BUILD_DIR/hyprexpo.so}"
CONF="${XDG_CACHE_HOME:-$HOME/.cache}/hyprexpo-dev.conf"
DEV_LAYOUT="${HYPREXPO_DEV_LAYOUT:-grid}"

case "$DEV_LAYOUT" in
    grid)
        MONITOR_RULE='monitor=,preferred,auto,auto'
        LAYOUT_BLOCK=''
        FIXTURE_BLOCK=''
        SCROLLING_INPUT_DEBUG=0
        ;;
    scrolling)
        MONITOR_RULE='monitor=,800x600@60,auto,1'
        SCROLLING_INPUT_DEBUG=1
        read -r -d '' LAYOUT_BLOCK <<'EOF' || true
general {
  layout = scrolling
  border_size = 0
  gaps_in = 8
  gaps_out = 8
}

scrolling {
  direction = right
  column_width = 0.42
  fullscreen_on_one_column = 0
  follow_focus = 0
}

# Four native direction fixtures plus one mixed-layout fallback row.
workspace = 1, layout:scrolling, layoutopt:direction:right
workspace = 2, layout:scrolling, layoutopt:direction:left
workspace = 3, layout:scrolling, layoutopt:direction:down
workspace = 4, layout:scrolling, layoutopt:direction:up
workspace = 5, layout:dwindle
EOF
        read -r -d '' FIXTURE_BLOCK <<'EOF' || true
# The first workspace settles to three columns: C+D share a column, A and B are
# dedicated column, and D remains offscreen at the default 0.42 width.
exec-once = [workspace 1 silent] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-SCROLL-A
exec-once = [workspace 1 silent] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-SCROLL-B
exec-once = [workspace 1 silent] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-SCROLL-C
exec-once = [workspace 1 silent] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-SCROLL-D
exec-once = [workspace 2 silent] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-SCROLL-LEFT
exec-once = [workspace 3 silent] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-SCROLL-DOWN
exec-once = [workspace 4 silent] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-SCROLL-UP
exec-once = [workspace 5 silent] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-MIXED
exec-once = [workspace 1 silent; float] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-FLOATING
exec-once = [workspace 1 silent; float] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-PINNED
exec-once = [workspace 5 silent] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-GROUP
exec-once = [workspace 5 silent] kitty --class hyprexpo-scroll-fixture --title HYPREXPO-FULLSCREEN
exec-once = sh -c 'sleep 2; hyprctl dispatch focuswindow title:HYPREXPO-SCROLL-D; hyprctl dispatch layoutmsg consume; hyprctl dispatch focuswindow title:HYPREXPO-PINNED; hyprctl dispatch pin; hyprctl dispatch focuswindow title:HYPREXPO-GROUP; hyprctl dispatch togglegroup; hyprctl dispatch focuswindow title:HYPREXPO-FULLSCREEN; hyprctl dispatch fullscreen 1; hyprctl dispatch workspace 1'
EOF
        ;;
    *)
        printf 'HYPREXPO_DEV_LAYOUT must be grid or scrolling, got: %s\n' "$DEV_LAYOUT" >&2
        exit 2
        ;;
esac

mkdir -p "$(dirname "$CONF")" "$(dirname "$SO")"
echo "[run-nested] Building local plugin at $SO"
make -C "$REPO_ROOT" all TARGET="$SO"

cat > "$CONF" <<EOF
$MONITOR_RULE

$LAYOUT_BLOCK

debug {
  disable_logs = false
}

cursor {
  no_hardware_cursors = true
}

# load local build
plugin = $SO

plugin {
  hyprexpo {
    # layout + visuals
    columns = 3
    gaps_in = 20
    bg_col = rgb(101010)
    workspace_method = center current
    skip_empty = 0
    show_pinned_windows = 0
    scrolling_thumbnail_budget = 4
    scrolling_input_debug = $SCROLLING_INPUT_DEBUG

    # borders (hypr-style gradient, thicker to showcase)
    border_style = hyprland
    border_width = 4
    border_color_current = rgb(66ccff)
    border_color_focus   = rgb(ffcc66)

    # keyboard nav
    keynav_enable = 1
    keynav_wrap_h = 1
    keynav_wrap_v = 1
    keynav_reading_order = 0

    # labels (numbers): smaller font, rounded background bubble
    label_enable = 1
    label_font_size = 12
    label_position = bottom-right
    label_offset_x = 8
    label_offset_y = 8
    label_show = hover+focus
    label_color_default = rgb(ffffff)
    label_color_hover   = rgb(72ff7a)
    label_color_focus   = rgb(ffcc66)
    label_color_current = rgb(66ccff)
    label_scale_hover = 1.0
    label_scale_focus = 1.2
    label_bg_enable = 1
    label_bg_color = rgba(000000cc)
    label_bg_rounding = 999  # fully rounded bubble
    label_padding = 6

    # outer margin around the grid to demo spacing from screen edge
    gaps_out = 20

    # demo hyprland style gradient borders
    border_grad_current = rgba(33ccffee) rgba(00ff99ee) 45deg
    border_grad_focus   = rgba(ffdd44ee) rgba(22aaffee) 30deg
  }
}

# toggle with an unmodified function key to avoid host grabs
bind = , F10, hyprexpo:expo, toggle

# nested-session test controls
bind = SUPER, Return, exec, kitty
bind = SUPER, Q, killactive
bind = SUPER SHIFT, Q, exit
bind = SUPER, 1, workspace, 1
bind = SUPER, 2, workspace, 2
bind = SUPER, 3, workspace, 3
bind = SUPER, 4, workspace, 4
bind = SUPER, 5, workspace, 5
bind = SUPER, 6, workspace, 6
bind = SUPER, 7, workspace, 7
bind = SUPER, 8, workspace, 8
bind = SUPER, 9, workspace, 9
bind = SUPER SHIFT, 1, movetoworkspace, 1
bind = SUPER SHIFT, 2, movetoworkspace, 2
bind = SUPER SHIFT, 3, movetoworkspace, 3
bind = SUPER SHIFT, 4, movetoworkspace, 4
bind = SUPER SHIFT, 5, movetoworkspace, 5
bind = SUPER SHIFT, 6, movetoworkspace, 6
bind = SUPER SHIFT, 7, movetoworkspace, 7
bind = SUPER SHIFT, 8, movetoworkspace, 8
bind = SUPER SHIFT, 9, movetoworkspace, 9

# Native scrolling layout controls used by the scrolling fixture and validator.
bind = SUPER ALT, left, layoutmsg, move -200
bind = SUPER ALT, right, layoutmsg, move +200
bind = SUPER ALT, C, layoutmsg, consume
bind = SUPER ALT, E, layoutmsg, expel
bind = SUPER ALT, F, layoutmsg, fit visible

# submap for keyboard nav (the plugin auto-enters this when open)
submap = hyprexpo
  bind = , left, hyprexpo:kb_focus, left
  bind = , right, hyprexpo:kb_focus, right
  bind = , up, hyprexpo:kb_focus, up
  bind = , down, hyprexpo:kb_focus, down
  bind = , return, hyprexpo:kb_confirm
  bind = , 1, hyprexpo:kb_selectn, 1
  bind = , 2, hyprexpo:kb_selectn, 2
  bind = , 3, hyprexpo:kb_selectn, 3
  bind = , 4, hyprexpo:kb_selectn, 4
  bind = , 5, hyprexpo:kb_selectn, 5
  bind = , 6, hyprexpo:kb_selectn, 6
  bind = , 7, hyprexpo:kb_selectn, 7
  bind = , 8, hyprexpo:kb_selectn, 8
  bind = , 9, hyprexpo:kb_selectn, 9
  bind = , 0, hyprexpo:kb_selectn, 0
submap = reset

$FIXTURE_BLOCK
EOF

echo "[run-nested] Launching $DEV_LAYOUT nested Hyprland with $CONF"
exec env WLR_BACKENDS=wayland WLR_RENDERER=pixman WLR_NO_HARDWARE_CURSORS=1 HYPRLAND_NO_LOGO=1 Hyprland -c "$CONF"
