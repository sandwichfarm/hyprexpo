#!/usr/bin/env bash
set -euo pipefail

# Rebuilds hyprexpo.so on source changes and relaunches a nested Hyprland
# that loads the local build. No impact on your main session.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/hyprexpo"
SO="${HYPREXPO_DEV_SO:-$BUILD_DIR/hyprexpo.so}"
CONF="${XDG_CACHE_HOME:-$HOME/.cache}/hyprexpo-dev.conf"

gen_conf() {
  mkdir -p "$(dirname "$CONF")"
  cat > "$CONF" <<EOF
monitor=,preferred,auto,auto

debug {
  disable_logs = false
}

cursor {
  no_hardware_cursors = true
}

plugin = $SO
plugin {
  hyprexpo {
    border_style = hyprland
    tile_rounding = 12
    tile_rounding_focus = 16
    tile_rounding_current = 14
    show_pinned_windows = 0
  }
}
bind = , F10, hyprexpo:expo, toggle
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
submap = hyprexpo
  bind = , left, hyprexpo:kb_focus, left
  bind = , right, hyprexpo:kb_focus, right
  bind = , up, hyprexpo:kb_focus, up
  bind = , down, hyprexpo:kb_focus, down
  bind = , return, hyprexpo:kb_confirm
submap = reset
EOF
}

launch_nested() {
  echo "[dev-watch] launching nested Hyprland"
  WLR_BACKENDS=wayland WLR_RENDERER=pixman WLR_NO_HARDWARE_CURSORS=1 HYPRLAND_NO_LOGO=1 Hyprland -c "$CONF" &
  NESTED_PID=$!
}

stop_nested() {
  if [[ -n "${NESTED_PID:-}" ]] && kill -0 "$NESTED_PID" 2>/dev/null; then
    echo "[dev-watch] stopping nested ($NESTED_PID)"
    kill "$NESTED_PID" 2>/dev/null || true
    wait "$NESTED_PID" 2>/dev/null || true
  fi
}

build() {
  mkdir -p "$(dirname "$SO")"
  echo "[dev-watch] building $SO"
  make -C "$REPO_ROOT" all TARGET="$SO" || { echo "[dev-watch] build failed"; return 1; }
}

trap stop_nested EXIT INT TERM
gen_conf
build
launch_nested

echo "[dev-watch] watching sources... (press Ctrl-C to stop)"
command -v inotifywait >/dev/null 2>&1 || { echo "[dev-watch] please install inotify-tools"; exit 1; }

# Watch directories so atomic replacements and newly created files remain visible.
inotifywait -qm -e close_write,move,create,delete --format '%w%f' \
  "$REPO_ROOT" "$REPO_ROOT/src" "$REPO_ROOT/tests" \
  | while read -r changed; do
      case "$changed" in
        "$REPO_ROOT"/src/*|"$REPO_ROOT"/tests/*|"$REPO_ROOT"/Makefile|"$REPO_ROOT"/meson.build|"$REPO_ROOT"/CMakeLists.txt|"$REPO_ROOT"/VERSION) ;;
        *) continue ;;
      esac
      echo "[dev-watch] change detected: $changed"
      if build; then
        stop_nested
        launch_nested
      fi
    done
