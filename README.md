# HyprExpo

Expose for HyprLand, the original HyprExpo fork [since long before it was retired](https://github.com/hyprwm/hyprland-plugins/pull/507#issuecomment-4433386463), born from a yearning for [substantially better functionality.](https://www.reddit.com/r/hyprland/comments/1o30dsg/hyprexpoplus_outer_gaps_keyboard_navigation_and/)

https://github.com/user-attachments/assets/861baa26-46b6-4fa8-8d37-65cbb9ecbed4

## Documentation and Site

The VitePress documentation source lives in `docs/`; the public SPA source lives
in `site/`. Build both static outputs into `dist/` with:

```bash
./scripts/build-site.sh
```

Preview the built output with:

```bash
./scripts/serve-site.sh
```

## Install

### Via `hyprpm`

```bash
hyprpm add https://github.com/sandwichfarm/hyprexpo
hyprpm enable hyprexpo
hyprpm reload
```

### Build from Source

Build dependencies are a C++23 compiler, `pkg-config`, Hyprland development
headers, and these pkg-config packages:

```text
hyprland pixman-1 libdrm pangocairo libinput libudev wayland-server xkbcommon lua5.4
```

Build with the Makefile:

```bash
git clone https://github.com/sandwichfarm/hyprexpo
cd hyprexpo
make all
```

Or with Meson:

```bash
meson setup build
meson compile -C build
```

Or with CMake:

```bash
cmake -S . -B build
cmake --build build
```

Verify a local build with the same command set used for release preparation:

```bash
make test
make all
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
meson setup build-meson
meson compile -C build-meson
meson test -C build-meson --print-errorlogs
```

To install the Makefile build over the `hyprpm` managed copy:

```bash
make install
hyprpm reload
```

Equivalent manual install:

```bash
install -Dm755 hyprexpo.so \
    /var/cache/hyprpm/$USER/hyprexpo/hyprexpo.so
hyprpm reload
```

> [!WARNING]
> Use `install`, not plain `cp`, when replacing the loaded `.so`. Hyprland maps
> the plugin into the running process. Overwriting the file in place can corrupt
> that live mapping and crash the session; `install` replaces the inode
> atomically.

### Nix

Nix users should build HyprExpo through the Nix Hyprland plugin path instead of
mixing a `hyprpm` artifact into a Nix-managed Hyprland session. The repository
contains `default.nix`, which uses `hyprlandPlugins.mkHyprlandPlugin` so the
plugin follows the Hyprland input supplied by the caller.

Hyprland plugins are ABI-sensitive. Keep the plugin build and running Hyprland
revision aligned; if Hyprland is updated, rebuild the plugin from the same
Hyprland input before loading it.

## Compatibility and Release Provenance

Hyprland checks plugin API compatibility when loading a plugin. If the plugin was
built against an incompatible Hyprland revision, loading should fail with a
visible API/hash mismatch instead of silently running against the wrong ABI.

Release builds attach `release-provenance.txt` next to `hyprexpo.so`. That file
records the packaged Hyprland version, Lua pkg-config version, compiler, and
`ldd -r` output for the release artifact.

### Runtime Smoke Checklist

Run these checks in a nested Hyprland session or another disposable compositor
session before publishing a release:

`scripts/run-nested.sh` launches a disposable nested session with a fresh
user-owned build under `${XDG_CACHE_HOME:-$HOME/.cache}/hyprexpo-plus`;
`scripts/dev-watch.sh` rebuilds and relaunches that session on source changes.
Nested test binds are `F10` for overview, `SUPER+Return` for a terminal,
`SUPER+1..9` for workspaces, `SUPER+SHIFT+1..9` to move a window, `SUPER+Q`
to close a window, and `SUPER+SHIFT+Q` to exit.

1. Load the locally built plugin and confirm no API/hash mismatch is reported.
2. Toggle overview on and off with `hyprexpo:expo, toggle`.
3. Cancel overview with the configured `cancel_key` and confirm the workspace
   does not change.
4. Move keyboard focus with `hyprexpo:kb_focus`, then select with
   `hyprexpo:kb_confirm`.
5. Select by pointer or touch near the outside edges of the monitor.
6. Confirm current, hover, focus, label, and border styling render as configured.
7. Exercise the Lua gesture registration path and complete/cancel a swipe.
8. Unload or reload the plugin after closing overview and confirm no stale render
   pass callback crashes the session.

## Quick Config

Add the plugin block to your Hyprland config:

```ini
plugin {
    hyprexpo {
        columns = 3
        gaps_in = 5
        gaps_out = 0
        bg_col = rgb(111111)
        workspace_method = center current
        gesture_distance = 200
        cancel_key = escape
        show_cursor = 1
    }
}
```

Add a dispatcher binding:

```ini
bind = SUPER, g, hyprexpo:expo, toggle
```

Optional keyboard navigation:

```ini
plugin {
    hyprexpo {
        keynav_enable = 1
        keynav_wrap_h = 1
        keynav_wrap_v = 1
        keynav_reading_order = 0
    }
}

submap = hyprexpo
    bind = , left,   hyprexpo:kb_focus, left
    bind = , right,  hyprexpo:kb_focus, right
    bind = , up,     hyprexpo:kb_focus, up
    bind = , down,   hyprexpo:kb_focus, down
    bind = , return, hyprexpo:kb_confirm
    bind = , escape, hyprexpo:expo, cancel
    bind = , 1,      hyprexpo:kb_selecti, 1
    bind = , 2,      hyprexpo:kb_selecti, 2
    bind = , 3,      hyprexpo:kb_selecti, 3
    bind = , 4,      hyprexpo:kb_selecti, 4
    bind = , 5,      hyprexpo:kb_selecti, 5
    bind = , 6,      hyprexpo:kb_selecti, 6
    bind = , 7,      hyprexpo:kb_selecti, 7
    bind = , 8,      hyprexpo:kb_selecti, 8
    bind = , 9,      hyprexpo:kb_selecti, 9
    bind = , 0,      hyprexpo:kb_selecti, 10
submap = reset
```

## Configuration Surface

Normal settings live under `plugin:hyprexpo:*`, either in the plugin block above
or with fully qualified Hyprland config keys.

Hyprland 0.55 deprecated the custom keyword API that older HyprExpo configs used.
HyprExpo no longer registers `hyprexpo_gesture` or
`hyprexpo_workspace_method`. Use `plugin:hyprexpo:workspace_method` for
workspace placement and the Lua API for gestures.

Color-backed settings such as `bg_col`, `label_color*`, `label_bg_color`, and
`selection_label_color` use Hyprland color parsing. Solid border colors are
strings because border settings also accept gradient specs.

## Options

### Layout and Behavior

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:columns` | int | desktops per row, clamped to `1..7` | `3` |
| `plugin:hyprexpo:gaps_in` | int | spacing between tiles in pixels | `5` |
| `plugin:hyprexpo:gaps_out` | int | outer margin around the grid in pixels | `0` |
| `plugin:hyprexpo:bg_col` | color | grid background color | `0xFF111111` |
| `plugin:hyprexpo:workspace_method` | string | placement: `center current` or `first <workspace>` | `center current` |
| `plugin:hyprexpo:skip_empty` | bool int | skip empty workspaces using selector `m` when enabled | `0` |
| `plugin:hyprexpo:max_workspace` | int | when `skip_empty = 0`, cap sequential overview tiles at this workspace ID; `0` keeps Hyprland selector behavior | `0` |
| `plugin:hyprexpo:gesture_distance` | int | swipe distance considered complete | `200` |
| `plugin:hyprexpo:cancel_key` | string | comma-separated key names that close overview without selecting; `none` or `off` disables | `escape` |
| `plugin:hyprexpo:show_cursor` | bool int | keep the cursor visible while overview is open; set `0` for old hidden-cursor behavior | `1` |

### Tile Appearance

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:tile_rounding` | int | corner radius in pixels | `0` |
| `plugin:hyprexpo:tile_rounding_power` | float | rounding curve exponent | `2.0` |
| `plugin:hyprexpo:tile_rounding_focus` | int | focused tile radius, `-1` inherits | `-1` |
| `plugin:hyprexpo:tile_rounding_current` | int | current tile radius, `-1` inherits | `-1` |
| `plugin:hyprexpo:tile_rounding_hover` | int | hovered tile radius, `-1` inherits | `-1` |
| `plugin:hyprexpo:border_width` | int | border thickness in pixels | `2` |
| `plugin:hyprexpo:border_color` | string | default border for non-highlighted tiles; solid or gradient | empty |
| `plugin:hyprexpo:border_color_current` | string | current tile border; solid or gradient | `rgb(66ccff)` |
| `plugin:hyprexpo:border_color_focus` | string | focused tile border; solid or gradient | `rgb(ffcc66)` |
| `plugin:hyprexpo:border_color_hover` | string | hovered tile border; solid or gradient | `rgb(aabbcc)` |
| `plugin:hyprexpo:border_grad_current` | string | deprecated fallback; use `border_color_current` | empty |
| `plugin:hyprexpo:border_grad_focus` | string | deprecated fallback; use `border_color_focus` | empty |
| `plugin:hyprexpo:border_grad_hover` | string | deprecated fallback; use `border_color_hover` | empty |
| `plugin:hyprexpo:border_style` | string | deprecated; border style is detected from the color format | `simple` |

Border values accept solid colors such as `rgb(66ccff)` or `0xFF66CCFF`.
Gradient border values use the Hyprland-style format:

```ini
border_color_current = rgba(33ccffee) rgba(00ff99ee) 45deg
```

### Workspace Labels

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:label_enable` | bool int | enable workspace labels | `1` |
| `plugin:hyprexpo:label_color` | color | legacy accepted label color; state-specific colors below control rendering | `0xFFFFFFFF` |
| `plugin:hyprexpo:label_text_mode` | string | `token`, `index`, or `id` | `token` |
| `plugin:hyprexpo:label_token_map` | string | comma-separated tokens by visible tile order; empty entries skip | empty |
| `plugin:hyprexpo:label_position` | string | `top-left`, `top-right`, `bottom-left`, `bottom-right`, or `center` | `center` |
| `plugin:hyprexpo:label_offset_x` | int | horizontal offset from anchor in pixels | `0` |
| `plugin:hyprexpo:label_offset_y` | int | vertical offset from anchor in pixels | `0` |
| `plugin:hyprexpo:label_show` | string | `always`, `hover`, `focus`, `hover+focus`, `current+focus`, or `never` | `always` |
| `plugin:hyprexpo:label_color_default` | color | default label text color | `rgb(ffffff)` |
| `plugin:hyprexpo:label_color_hover` | color | hovered label text color | `rgb(eeeeee)` |
| `plugin:hyprexpo:label_color_focus` | color | focused label text color | `rgb(ffcc66)` |
| `plugin:hyprexpo:label_color_current` | color | current workspace label text color | `rgb(66ccff)` |
| `plugin:hyprexpo:show_workspace_numbers` | bool int | force labels to show workspace IDs regardless of `label_text_mode` | `0` |
| `plugin:hyprexpo:workspace_number_color` | color | forced workspace ID label color | `rgb(ffffff)` |
| `plugin:hyprexpo:label_scale_hover` | float | hover scale multiplier | `1.0` |
| `plugin:hyprexpo:label_scale_focus` | float | focus scale multiplier | `1.0` |
| `plugin:hyprexpo:label_font_size` | int | base font size in pixels | `16` |
| `plugin:hyprexpo:label_font_family` | string | Pango font family | `sans` |
| `plugin:hyprexpo:label_font_bold` | bool int | bold text | `0` |
| `plugin:hyprexpo:label_font_italic` | bool int | italic text | `0` |
| `plugin:hyprexpo:label_text_underline` | bool int | underline text | `0` |
| `plugin:hyprexpo:label_text_strikethrough` | bool int | strikethrough text | `0` |
| `plugin:hyprexpo:label_pixel_snap` | bool int | snap label positions to whole pixels | `1` |
| `plugin:hyprexpo:label_center_adjust_x` | int | manual center nudge in pixels | `0` |
| `plugin:hyprexpo:label_center_adjust_y` | int | manual center nudge in pixels | `0` |
| `plugin:hyprexpo:label_bg_enable` | bool int | draw a background bubble behind labels | `1` |
| `plugin:hyprexpo:label_bg_color` | color | label background color | `rgba(00000088)` |
| `plugin:hyprexpo:label_bg_shape` | string | `circle`, `square`, or `rounded` | `circle` |
| `plugin:hyprexpo:label_bg_rounding` | int | radius for `rounded` backgrounds | `8` |
| `plugin:hyprexpo:label_padding` | int | background padding around text in pixels | `8` |

### Selection Labels

Selection labels are optional overlays used by `hyprexpo:kb_select`. They let
normal workspace labels stay stable while selection tokens use a separate map.

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:selection_label_enable` | bool int | enable the separate selection-token overlay | `0` |
| `plugin:hyprexpo:selection_label_token_map` | string | comma-separated tokens by visible tile order; empty entries skip | `a,s,d,f,g,q,w,e,r,t,z,x,c,v,b` |
| `plugin:hyprexpo:selection_label_position` | string | `top-left`, `top-right`, `bottom-left`, `bottom-right`, or `center` | `top-right` |
| `plugin:hyprexpo:selection_label_offset_x` | int | horizontal offset from anchor in pixels | `6` |
| `plugin:hyprexpo:selection_label_offset_y` | int | vertical offset from anchor in pixels | `6` |
| `plugin:hyprexpo:selection_label_color` | color | selection-token text color | `rgb(ffcc66)` |

### Keyboard Navigation

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:keynav_enable` | bool int | enable keyboard navigation and the overview submap | `1` |
| `plugin:hyprexpo:keynav_wrap_h` | bool int | wrap horizontally at row edges | `1` |
| `plugin:hyprexpo:keynav_wrap_v` | bool int | wrap vertically at column edges | `1` |
| `plugin:hyprexpo:keynav_reading_order` | bool int | use row-major horizontal movement | `0` |

## Dispatchers

Main dispatcher:

```ini
bind = SUPER, g, hyprexpo:expo, toggle
```

| option | description |
| --- | --- |
| `toggle` | show overview if hidden, hide it if shown |
| `on` or `enable` | show overview |
| `off` or `disable` | hide overview |
| `cancel` | hide overview without switching workspaces |
| `select` | select the hovered workspace |
| `bring` | move the top mapped window from the hovered workspace into the current workspace |
| `1`..`9` | select that workspace by ID while overview is open; otherwise dispatch a normal workspace switch |

Keyboard navigation dispatchers are active during overview:

| dispatcher | argument | description |
| --- | --- | --- |
| `hyprexpo:kb_focus` | `left`, `right`, `up`, or `down` | move focus across tiles |
| `hyprexpo:kb_confirm` | none | select the focused tile |
| `hyprexpo:kb_selecti` | index | select by 1-based visible index |
| `hyprexpo:kb_selectn` | workspace ID | select by workspace ID; `0` maps to workspace `10` |
| `hyprexpo:kb_select` | token | select by token; uses the selection-label map when enabled |
| `hyprexpo:move_window` | `source target [address]` | move a window between 1-based visible tile indices; defaults to the top mapped source-window |

Hyprland may briefly report invalid dispatcher messages during startup if binds
are parsed before plugins are loaded. Those messages are cosmetic; the
dispatchers work once the plugin is loaded.

## Lua Config Support

When Hyprland is using Lua config support, HyprExpo exposes functions under
`hl.plugin.hyprexpo`:

```lua
hl.plugin.hyprexpo.expo("toggle")
hl.plugin.hyprexpo.expo("cancel")
hl.plugin.hyprexpo.kb_focus("left")
hl.plugin.hyprexpo.kb_confirm()
hl.plugin.hyprexpo.kb_selecti(1)
hl.plugin.hyprexpo.kb_selectn(1)
hl.plugin.hyprexpo.kb_select("1")
```

Lua arguments are validated strictly:

| function | accepted arguments |
| --- | --- |
| `expo` | string, defaulting to `"toggle"` when omitted |
| `kb_focus` | string |
| `kb_confirm` | none |
| `kb_selecti` | Lua integer or exact integer string |
| `kb_selectn` | Lua integer or exact integer string |
| `kb_select` | string |

Fractional numbers, booleans, tables where strings are expected, and partial
numeric strings are rejected instead of being silently coerced.

## Gestures

Gestures are configured from Lua:

```lua
hl.plugin.hyprexpo.gesture({
    fingers = 4,
    direction = "up",
    action = "expo",
})
```

`gesture` accepts this table shape:

| field | type | required | description |
| --- | --- | --- | --- |
| `fingers` | integer | yes | number of fingers |
| `direction` | string | yes | swipe direction |
| `action` | string | no | `expo` or `unset`; defaults to `expo` |
| `mods` | string | no | modifier expression passed to Hyprland |
| `scale` | number | no | gesture scale; defaults to `1.0` |
| `disable_inhibit` | boolean | no | whether to bypass inhibit handling |

This replaces the legacy `hyprexpo_gesture` custom keyword while preserving the
same live swipe behavior.

## Migration From Legacy Keywords

Legacy `hyprexpo_workspace_method` and `hyprexpo_gesture` lines are no longer
registered on Hyprland 0.55+. Use the migration tool when updating older
configs.

Dry run:

```bash
python3 tools/hyprexpo-migrate-config.py ~/.config/hypr/hyprland.conf
```

Dry-run behavior:

- Does not modify config files.
- Prints the unified diff to stdout.
- Prints generated Lua gesture snippets and warnings to stderr.
- Ignores `--lua-out` unless `--write` is also set, so dry runs do not create
  files.

Write changes:

```bash
python3 tools/hyprexpo-migrate-config.py \
    --write \
    --lua-out ~/.config/hypr/hyprexpo.lua \
    ~/.config/hypr/hyprland.conf
```

Write behavior:

- Rewrites each listed config file in place.
- Creates `*.bak` files before overwriting unless `--no-backup` is set.
- Writes generated Lua gesture snippets to `--lua-out` when provided.
- Backs up an existing `--lua-out` file before overwriting unless `--no-backup`
  is set.
- Only processes files named on the command line; it does not follow Hyprland
  `source = ...` includes.

Example input:

```ini
hyprexpo_workspace_method = DP-1 first 1
hyprexpo_workspace_method = HDMI-A-1 center 5
hyprexpo_gesture = swipe:4:up, hyprexpo:expo, toggle
```

Migrated Hyprland config:

```ini
plugin {
    hyprexpo {
        workspace_method = DP-1 first 1, HDMI-A-1 center 5
    }
}
```

Generated Lua:

```lua
hl.plugin.hyprexpo.gesture({ fingers = 4, direction = "up", action = "expo" })
```

Gesture lines that cannot be parsed are commented with a manual-migration TODO
instead of being deleted.

## Per-Monitor Workspace Method

`plugin:hyprexpo:workspace_method` can be global, per-monitor, or a mix of
per-monitor entries with a global fallback.

Formats:

```text
center <workspace>
first <workspace>
MONITOR center <workspace>
MONITOR first <workspace>
```

Separate multiple entries with commas:

```ini
plugin {
    hyprexpo {
        workspace_method = DP-1 first 1, HDMI-1 center 5, eDP-1 first 10
    }
}
```

Mixed monitor-specific entries and fallback:

```ini
plugin {
    hyprexpo {
        workspace_method = DP-1 first 1, center current
    }
}
```

## Troubleshooting

- Plugin load fails with an API or hash mismatch: rebuild HyprExpo against the
  same Hyprland revision that is running, then reload the plugin.
- Plugin load fails because dependencies are missing: install the build
  dependencies listed above and rebuild. `lua5.4`, `pangocairo`, and `xkbcommon`
  are linked by the current plugin.
- Invalid `workspace_method` or border color values: HyprExpo logs the invalid
  value and falls back safely. Use the formats shown in the sections above.
- `columns` is outside the supported range: the runtime clamps it to `1..7` so
  pointer and keyboard selection stay in bounds.
- Replacing a loaded `.so` crashes Hyprland: use `make install` or `install`
  instead of overwriting the file with `cp`.
- Per-monitor workspace placement does not apply: check the monitor name from
  Hyprland and use comma-separated entries such as
  `DP-1 first 1, HDMI-1 center 5, center current`.
