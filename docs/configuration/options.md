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
| `plugin:hyprexpo:cancel_key` | string | comma-separated key names that close overview without selecting; `none` or `off` disables | `escape` |
| `plugin:hyprexpo:show_cursor` | bool int | keep the cursor visible while overview is open; set `0` for old hidden-cursor behavior | `1` |

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
workspace previews. Empty border color values inherit the focused tile border,
so the default look stays unchanged until you opt into drag/drop-specific
styling.

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
border colors, and gradient values are expected to fail safely. The plugin
should log invalid values or use a fallback instead of crashing Hyprland during
render.
