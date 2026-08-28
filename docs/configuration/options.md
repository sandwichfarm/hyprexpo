# Configuration Options

This is the complete runtime option reference for `plugin:hyprexpo:*` settings.
Use the short key names inside a `plugin { hyprexpo { ... } }` block, or use the
fully qualified names when setting options elsewhere in Hyprland config.

Hyprland 0.55 deprecated the custom keyword API that older HyprExpo configs
used. HyprExpo no longer registers `hyprexpo_gesture` or
`hyprexpo_workspace_method`. Use `plugin:hyprexpo:workspace_method` for
workspace placement and the Lua API for gestures.

## Example Shape

```ini
plugin {
    hyprexpo {
        columns = 3
        gaps_in = 5
        gaps_out = 0
        bg_col = rgb(111111)
        workspace_method = center current
        keynav_enable = 1
        label_enable = 1
        border_width = 2
    }
}
```

In `hyprland.lua`, use `hl.config()` with a nested `plugin.hyprexpo` table:

```lua
hl.config({
    plugin = {
        hyprexpo = {
            columns = 3,
            gaps_in = 5,
            gaps_out = 0,
            bg_col = "rgb(111111)",
            workspace_method = "center current",
            keynav_enable = 1,
            label_enable = 1,
            border_width = 2,
        },
    },
})
```

`hl.plugin.hyprexpo` is the Lua helper namespace for dispatchers and gestures;
it is not the configuration block.

## PR #605 Compatibility Keys

These keys are registered for the active-overview port and are intentionally
separate from the richer sandwichfarm config surface. They are kept for later
integration and should not replace the newer keys below.

| key | type | legacy meaning | newer sandwichfarm counterpart |
| --- | --- | --- | --- |
| `plugin:hyprexpo:dynamic_grid` | int | opt-in dynamic workspace enumeration (default: off) | no direct equivalent yet |
| `plugin:hyprexpo:fill_gaps` | int | expand min..max workspace IDs when dynamic | no direct equivalent yet |
| `plugin:hyprexpo:mru_sort` | int | put the current workspace first | no direct equivalent yet |
| `plugin:hyprexpo:active_highlight_col` | color | active tile highlight color | `border_color_current` |
| `plugin:hyprexpo:active_highlight_border` | int | active tile highlight width | `border_width` |
| `plugin:hyprexpo:hover_highlight_col` | color | hovered tile highlight color | `border_color_hover` |
| `plugin:hyprexpo:hover_highlight_border` | int | hovered tile highlight width | `border_width` |
| `plugin:hyprexpo:label_pos` | string | legacy underscore anchor such as `top_right` | `label_position` (`top-right`) |
| `plugin:hyprexpo:label_size` | int | legacy badge size; text renders at roughly half this size | `label_font_size` |
| `plugin:hyprexpo:label_col` | color | legacy label tint | `label_color` / `label_color_default` |
| `plugin:hyprexpo:show_workspace_names` | int | legacy name/number toggle | `show_workspace_numbers` and `label_text_mode` |
| `plugin:hyprexpo:enable_keyboard_nav` | int | legacy keyboard-navigation toggle | `keynav_enable` |
| `plugin:hyprexpo:enable_drag_move` | int | legacy drag-move toggle | no direct equivalent yet |
| `plugin:hyprexpo:animate_entry` | int | legacy open animation toggle | no direct equivalent yet |
| `plugin:hyprexpo:wallpaper_bg` | int | draw the monitor wallpaper behind the overview tiles | optional active-overview extension |

Enable the active-workspace layout explicitly; all compatibility options default
to the existing fixed-grid behavior:

```ini
plugin {
    hyprexpo {
        dynamic_grid = 1
        fill_gaps = 0
        mru_sort = 0
        show_workspace_names = 1
        label_pos = top_right
        label_size = 48
        wallpaper_bg = 1
    }
}
```

## Layout and Behavior

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
| `plugin:hyprexpo:gesture_fingers` | int | fingers for the interactive swipe gesture; `0` disables, otherwise `2`-`9` | `0` |
| `plugin:hyprexpo:gesture_direction` | string | swipe direction: `up`, `down`, `left`, `right`, `vertical`, `horizontal`, `pinch` | `up` |
| `plugin:hyprexpo:cancel_key` | string | comma-separated key names that close overview without selecting; `none` or `off` disables | `escape` |
| `plugin:hyprexpo:show_cursor` | bool int | keep the cursor visible while overview is open; set `0` for old hidden-cursor behavior | `1` |
| `plugin:hyprexpo:show_pinned_windows` | bool int | render pinned/PiP windows in workspace preview thumbnails; default `0` hides them from previews only | `0` |

Pinned windows, including browser Picture-in-Picture windows, stay pinned and visible in normal Hyprland. By default HyprExpo hides them only while capturing workspace preview thumbnails so they do not appear on every tile. Set `show_pinned_windows = 1` to opt in to the old preview behavior.

### Trackpad gesture

`gesture_fingers` and `gesture_direction` register the interactive, follow-your-finger `expo` gesture from plain config. They remain expo-only; registering the `cancel` action requires [`hl.plugin.hyprexpo.gesture{}`](../guides/lua-gestures.md) in a Lua config. Hyprland selects either hyprlang or Lua for the whole config and a `.lua` cannot be sourced from a `.conf`, so hyprlang users need these keys to reach the expo gesture at all.

```ini
plugin {
    hyprexpo {
        gesture_fingers = 3
        gesture_direction = vertical
    }
}
```

`gesture_fingers = 0` (the default) registers nothing, leaving trackpad handling entirely to Hyprland and Lua. Use a `gesture_direction` that does not collide with an existing Hyprland `gesture =` binding for the same finger count.

Bad values are rejected when the gesture is registered, not while the config is parsed. An unknown `gesture_direction`, or a `gesture_fingers` value that is neither `0` nor in `2`-`9`, raises a notification and leaves the gesture unregistered rather than failing silently. Under the hyprlang backend these do not reach `hyprctl configerrors`: plugins have no API for adding entries there, and hyprlang does not run the validators attached to plugin config values.

## Tile Appearance

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:tile_rounding` | int | corner radius in pixels for workspace previews | `0` |
| `plugin:hyprexpo:tile_rounding_power` | float | rounding curve exponent | `2.0` |
| `plugin:hyprexpo:tile_rounding_focus` | int | focused tile radius; `-1` inherits `tile_rounding` | `-1` |
| `plugin:hyprexpo:tile_rounding_current` | int | current tile radius; `-1` inherits `tile_rounding` | `-1` |
| `plugin:hyprexpo:tile_rounding_hover` | int | hovered tile radius; `-1` inherits `tile_rounding` | `-1` |
| `plugin:hyprexpo:border_width` | int | border thickness in pixels | `2` |
| `plugin:hyprexpo:border_color` | string | default border for non-highlighted tiles; solid color or gradient | empty |
| `plugin:hyprexpo:border_color_current` | string | current workspace tile border; solid color or gradient | `rgb(66ccff)` |
| `plugin:hyprexpo:border_color_focus` | string | keyboard-focused tile border; solid color or gradient | `rgb(ffcc66)` |
| `plugin:hyprexpo:border_color_hover` | string | pointer-hovered tile border; solid color or gradient | `rgb(aabbcc)` |
| `plugin:hyprexpo:border_grad_current` | string | deprecated current-tile gradient fallback; use `border_color_current` | empty |
| `plugin:hyprexpo:border_grad_focus` | string | deprecated focused-tile gradient fallback; use `border_color_focus` | empty |
| `plugin:hyprexpo:border_grad_hover` | string | deprecated hovered-tile gradient fallback; use `border_color_hover` | empty |
| `plugin:hyprexpo:border_style` | string | deprecated compatibility key; border style is detected from the color format | `simple` |

## Drag-Drop Window Styling

These options style the visual feedback shown while dragging a window between
workspace previews. HyprExpo renders the under-pointer drag proxy and, while the
pointer is over a valid target workspace, a positional landing proxy inside that
target tile. Empty border color values inherit the focused tile border, so the
default look stays unchanged until you opt into drag/drop-specific styling.

The landing proxy is visual-only in this milestone. It previews drop intent; the
actual release still uses the existing safe workspace move behavior.

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:drag_drop_proxy_color` | color | translucent dragged-window proxy before the move threshold is crossed | `0x24EDB342` |
| `plugin:hyprexpo:drag_drop_proxy_active_color` | color | dragged-window proxy after movement is active | `0x3DEDB342` |
| `plugin:hyprexpo:drag_drop_proxy_border_color` | string | proxy border; accepts solid colors or gradients, empty inherits `border_color_focus` / `border_grad_focus` | empty |
| `plugin:hyprexpo:drag_drop_proxy_border_width` | int | proxy border width; `-1` inherits `max(2, border_width + 1)`, `0` disables | `-1` |
| `plugin:hyprexpo:drag_drop_proxy_rounding` | int | proxy corner radius in pixels; `-1` inherits the automatic focused-tile rounding | `-1` |
| `plugin:hyprexpo:drag_drop_source_border_color` | string | source workspace border while a drag/drop move is active; accepts solid colors or gradients, empty inherits focus border | empty |
| `plugin:hyprexpo:drag_drop_source_border_width` | int | source workspace border width while dragging; `-1` inherits `border_width`, `0` disables | `-1` |

## Color Values

Color-backed settings such as `bg_col`, `label_color*`, `label_bg_color`, and
`selection_label_color` use Hyprland color parsing. Solid border colors are
strings because border settings also accept gradient specs.

Solid values:

```ini
bg_col = rgb(111111)
border_color_current = rgb(66ccff)
```

Gradient values:

```ini
border_color_current = rgba(33ccffee) rgba(00ff99ee) 45deg
```

Drag/drop styling values:

```ini
drag_drop_proxy_color = rgba(66ccff22)
drag_drop_proxy_active_color = rgba(66ccff44)
drag_drop_proxy_border_color = rgba(66ccffee) rgba(ffcc66ee) 45deg
drag_drop_source_border_color = rgb(ffcc66)
drag_drop_proxy_border_width = 3
drag_drop_proxy_rounding = 10
```

## Workspace Labels

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:label_enable` | bool int | enable workspace labels | `1` |
| `plugin:hyprexpo:label_color` | color | legacy accepted label color; state-specific colors below control rendering | `0xFFFFFFFF` |
| `plugin:hyprexpo:label_text_mode` | string | label text source: `token`, `index`, or `id` | `token` |
| `plugin:hyprexpo:label_token_map` | string | comma-separated tokens by visible tile order; empty entries skip | empty |
| `plugin:hyprexpo:label_position` | string | label anchor: `top-left`, `top-right`, `bottom-left`, `bottom-right`, or `center` | `center` |
| `plugin:hyprexpo:label_offset_x` | int | horizontal offset from the label anchor in pixels | `0` |
| `plugin:hyprexpo:label_offset_y` | int | vertical offset from the label anchor in pixels | `0` |
| `plugin:hyprexpo:label_show` | string | visibility rule: `always`, `hover`, `focus`, `hover+focus`, `current+focus`, or `never` | `always` |
| `plugin:hyprexpo:label_color_default` | color | default label text color | `rgb(ffffff)` |
| `plugin:hyprexpo:label_color_hover` | color | hovered label text color | `rgb(eeeeee)` |
| `plugin:hyprexpo:label_color_focus` | color | keyboard-focused label text color | `rgb(ffcc66)` |
| `plugin:hyprexpo:label_color_current` | color | current workspace label text color | `rgb(66ccff)` |
| `plugin:hyprexpo:show_workspace_numbers` | bool int | force labels to show workspace IDs regardless of `label_text_mode` | `0` |
| `plugin:hyprexpo:workspace_number_color` | color | forced workspace ID label color | `rgb(ffffff)` |
| `plugin:hyprexpo:label_scale_hover` | float | hover scale multiplier for labels | `1.0` |
| `plugin:hyprexpo:label_scale_focus` | float | keyboard-focus scale multiplier for labels | `1.0` |
| `plugin:hyprexpo:label_font_size` | int | base label font size in pixels | `16` |
| `plugin:hyprexpo:label_font_family` | string | Pango font family | `sans` |
| `plugin:hyprexpo:label_font_bold` | bool int | render label text in bold | `0` |
| `plugin:hyprexpo:label_font_italic` | bool int | render label text in italic | `0` |
| `plugin:hyprexpo:label_text_underline` | bool int | underline label text | `0` |
| `plugin:hyprexpo:label_text_strikethrough` | bool int | strikethrough label text | `0` |
| `plugin:hyprexpo:label_pixel_snap` | bool int | snap label positions to whole pixels | `1` |
| `plugin:hyprexpo:label_center_adjust_x` | int | manual center nudge in pixels for centered labels | `0` |
| `plugin:hyprexpo:label_center_adjust_y` | int | manual center nudge in pixels for centered labels | `0` |
| `plugin:hyprexpo:label_bg_enable` | bool int | draw a background bubble behind labels | `1` |
| `plugin:hyprexpo:label_bg_color` | color | label background color | `rgba(00000088)` |
| `plugin:hyprexpo:label_bg_shape` | string | label background shape: `circle`, `square`, or `rounded` | `circle` |
| `plugin:hyprexpo:label_bg_rounding` | int | radius for `rounded` label backgrounds | `8` |
| `plugin:hyprexpo:label_padding` | int | background padding around label text in pixels | `8` |

Label token maps are comma-separated and follow visible tile order:

```ini
label_text_mode = token
label_token_map = 1,2,3,4,5,6,7,8,9,0
```

## Selection Labels

Selection labels are optional overlays used by `hyprexpo:kb_select`. They let
normal workspace labels stay stable while selection tokens use a separate map.

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:selection_label_enable` | bool int | enable the separate selection-token overlay | `0` |
| `plugin:hyprexpo:selection_label_token_map` | string | comma-separated tokens by visible tile order; empty entries skip | `a,s,d,f,g,q,w,e,r,t,z,x,c,v,b` |
| `plugin:hyprexpo:selection_label_position` | string | selection label anchor: `top-left`, `top-right`, `bottom-left`, `bottom-right`, or `center` | `top-right` |
| `plugin:hyprexpo:selection_label_offset_x` | int | horizontal offset from the selection-label anchor in pixels | `6` |
| `plugin:hyprexpo:selection_label_offset_y` | int | vertical offset from the selection-label anchor in pixels | `6` |
| `plugin:hyprexpo:selection_label_color` | color | selection-token text color | `rgb(ffcc66)` |

Example token-based selection:

```ini
selection_label_enable = 1
selection_label_token_map = a,s,d,f,g,q,w,e,r,t
```

Then bind `hyprexpo:kb_select` to those tokens in the overview submap.

## Keyboard Navigation

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:keynav_enable` | bool int | enable keyboard navigation and the overview submap behavior | `1` |
| `plugin:hyprexpo:number_key_mode` | string | raw digit handling: `workspace`, `index`, or `passthrough` for user mappings | `workspace` |
| `plugin:hyprexpo:keynav_wrap_h` | bool int | wrap horizontally at row edges | `1` |
| `plugin:hyprexpo:keynav_wrap_v` | bool int | wrap vertically at column edges | `1` |
| `plugin:hyprexpo:keynav_reading_order` | bool int | use row-major horizontal movement instead of spatial movement | `0` |

## Related References

- [Labels and borders](./labels-borders) shows focused examples for appearance
  tuning.
- [Keyboard navigation](./keyboard) shows the matching submap binds.
- [Lua gestures](../guides/lua-gestures) documents `hl.plugin.hyprexpo.gesture`.
- [Dispatchers](../reference/dispatchers) documents `hyprexpo:*` dispatchers.

## Safe Failure Behavior

Invalid `columns`, workspace methods, label tokens, border colors, drag/drop
border colors, gradient values, and bool-int options are expected to fail
safely. The plugin should log invalid values or use a fallback instead of
crashing Hyprland during render.
