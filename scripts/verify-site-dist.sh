#!/usr/bin/env bash
set -euo pipefail

require_file() {
    local path="$1"
    if [ ! -f "$path" ]; then
        echo "Missing required output: $path" >&2
        exit 1
    fi
}

require_grep() {
    local pattern="$1"
    local path="$2"
    if ! grep -R -q "$pattern" "$path"; then
        echo "Expected pattern not found in $path: $pattern" >&2
        exit 1
    fi
}

require_file dist/index.html
require_file dist/docs/index.html
require_file dist/docs/getting-started/installation/index.html
require_file dist/docs/configuration/options/index.html
require_file dist/docs/guides/runtime-smoke/index.html
require_file dist/docs/reference/compatibility/index.html

require_grep "hyprpm add https://github.com/sandwichfarm/hyprexpo" dist/index.html
require_grep "reddit.com/r/hyprland/comments/1o30dsg" dist/index.html
require_grep "github.com/sandwichfarm/hyprexpo" dist/index.html
require_grep "github.com/user-attachments/assets/861baa26-46b6-4fa8-8d37-65cbb9ecbed4" dist/index.html
require_grep "drag-drop window movement" dist/index.html
require_grep "plugin:hyprexpo:show_cursor" dist/docs
require_grep "plugin:hyprexpo:tile_rounding" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:border_color_current" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:drag_drop_proxy_color" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:drag_drop_proxy_active_color" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:drag_drop_proxy_border_color" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:drag_drop_proxy_border_width" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:drag_drop_proxy_rounding" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:drag_drop_source_border_color" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:drag_drop_source_border_width" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:drag_drop_proxy_color" dist/docs/configuration/labels-borders/index.html
require_grep "plugin:hyprexpo:label_font_family" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:label_bg_shape" dist/docs/configuration/options/index.html
require_grep "plugin:hyprexpo:selection_label_offset_x" dist/docs/configuration/options/index.html
require_grep "hl.plugin.hyprexpo" dist/docs
require_grep "gesture" dist/docs/guides/lua-gestures.html
require_grep "HyprExpo" dist/index.html

if find dist -name '*.mdx' -print -quit | grep -q .; then
    echo "Unexpected MDX source file emitted to dist" >&2
    exit 1
fi

echo "Site dist verification passed."
