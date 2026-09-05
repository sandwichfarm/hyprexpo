#!/usr/bin/env bash
set -euo pipefail

# Launch a nested Hyprland session that loads the local hyprexpo.so,
# so you can test changes without restarting your main session.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/hyprexpo"
SO="${HYPREXPO_DEV_SO:-$BUILD_DIR/hyprexpo.so}"
CONF="${XDG_CACHE_HOME:-$HOME/.cache}/hyprexpo-dev.conf"

# A nested Wayland output advertises no preferred mode, so "preferred" resolves
# to 0x0 and Hyprland refuses to render it. Always pin an explicit mode.
MODE="${HYPREXPO_DEV_MODE:-1280x720@60}"
MODE_W="${MODE%%x*}"

# Number of nested outputs. Set to 2+ to exercise multi-monitor behavior; each
# extra output is created at runtime and appears as its own host window.
OUTPUTS="${HYPREXPO_DEV_OUTPUTS:-1}"

# Pick a terminal that actually exists on this machine instead of assuming one.
TERMINAL="${HYPREXPO_DEV_TERMINAL:-}"
if [[ -z "$TERMINAL" ]]; then
  for candidate in kitty ghostty alacritty foot wezterm; do
    if command -v "$candidate" >/dev/null 2>&1; then
      TERMINAL="$candidate"
      break
    fi
  done
fi
if [[ -z "$TERMINAL" ]]; then
  echo "[run-nested] warning: no terminal found; SUPER+Return will do nothing" >&2
  TERMINAL="kitty"
fi

EXTRA_OUTPUTS=""
for ((i = 2; i <= OUTPUTS; i++)); do
  EXTRA_OUTPUTS+="exec-once = hyprctl output create auto"$'\n'
done

mkdir -p "$(dirname "$CONF")" "$(dirname "$SO")"
echo "[run-nested] Building local plugin at $SO"
make -C "$REPO_ROOT" all TARGET="$SO"

cat > "$CONF" <<EOF
monitor=WAYLAND-1,$MODE,0x0,1
monitor=WAYLAND-2,$MODE,${MODE_W}x0,1
monitor=,$MODE,auto,1

$EXTRA_OUTPUTS

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
bind = , F11, hyprexpo:expo, toggle all

# nested-session test controls
bind = SUPER, Return, exec, $TERMINAL
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
EOF

echo "[run-nested] Launching nested Hyprland with $CONF"
# Hyprland 0.56 uses aquamarine, not wlroots: the old WLR_* variables are
# inert. Aquamarine selects its Wayland backend from WAYLAND_DISPLAY.
exec env HYPRLAND_NO_LOGO=1 Hyprland -c "$CONF"
