# Labels and Borders

HyprExpo can render workspace labels, separate selection labels, and distinct borders for current, focused, and hovered tiles.

## Tile Appearance

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

Deprecated fallback keys are still recognized for compatibility: `border_grad_current`, `border_grad_focus`, `border_grad_hover`, and `border_style`. New configs should use `border_color_*`.

## Drag-Drop Window Styling

Drag-drop window movement uses a translucent proxy under the pointer, a
source-workspace border, and a positional landing proxy inside the hovered
target tile while the move is active. By default those visuals keep the existing
proxy colors and inherit the focused tile border; set the `drag_drop_*` keys
when you want the window-moving feedback to match your theme.

The target-tile landing proxy is a visual drop-intent preview. It shows where
the grabbed window would land based on pointer position and grab offset, but the
current release behavior still uses Hyprland's safe workspace move path. Floating
positional release and tiled layout-aware insertion are deferred follow-up work.

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:drag_drop_proxy_color` | color | proxy fill before the drag crosses the move threshold | `0x24EDB342` |
| `plugin:hyprexpo:drag_drop_proxy_active_color` | color | proxy fill while a drag/drop move is active | `0x3DEDB342` |
| `plugin:hyprexpo:drag_drop_proxy_border_color` | string | proxy border, solid or gradient; empty inherits focus border | empty |
| `plugin:hyprexpo:drag_drop_proxy_border_width` | int | proxy border width; `-1` inherits, `0` disables | `-1` |
| `plugin:hyprexpo:drag_drop_proxy_rounding` | int | proxy corner radius; `-1` inherits automatic focused rounding | `-1` |
| `plugin:hyprexpo:drag_drop_source_border_color` | string | source workspace border during active drag/drop movement | empty |
| `plugin:hyprexpo:drag_drop_source_border_width` | int | source workspace border width; `-1` inherits, `0` disables | `-1` |

Example:

```ini
plugin {
    hyprexpo {
        drag_drop_proxy_color = rgba(66ccff22)
        drag_drop_proxy_active_color = rgba(66ccff44)
        drag_drop_proxy_border_color = rgba(66ccffee) rgba(ffcc66ee) 45deg
        drag_drop_source_border_color = rgb(ffcc66)
        drag_drop_proxy_border_width = 3
        drag_drop_proxy_rounding = 10
    }
}
```

## Workspace Labels

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:label_enable` | bool int | enable workspace labels | `1` |
| `plugin:hyprexpo:label_text_mode` | string | `token`, `index`, or `id` | `token` |
| `plugin:hyprexpo:label_token_map` | string | comma-separated tokens by visible tile order; empty entries skip | empty |
| `plugin:hyprexpo:label_position` | string | `top-left`, `top-right`, `bottom-left`, `bottom-right`, or `center` | `center` |
| `plugin:hyprexpo:label_show` | string | `always`, `hover`, `focus`, `hover+focus`, `current+focus`, or `never` | `always` |
| `plugin:hyprexpo:label_font_size` | int | base font size in pixels | `16` |
| `plugin:hyprexpo:label_font_family` | string | Pango font family | `sans` |
| `plugin:hyprexpo:label_bg_enable` | bool int | draw a background bubble behind labels | `1` |
| `plugin:hyprexpo:label_bg_color` | color | label background color | `rgba(00000088)` |
| `plugin:hyprexpo:label_bg_shape` | string | `circle`, `square`, or `rounded` | `circle` |

## Selection Labels

Selection labels are optional overlays used by `hyprexpo:kb_select`. They let normal workspace labels stay stable while selection tokens use a separate map.

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:selection_label_enable` | bool int | enable the separate selection-token overlay | `0` |
| `plugin:hyprexpo:selection_label_token_map` | string | comma-separated tokens by visible tile order; empty entries skip | `a,s,d,f,g,q,w,e,r,t,z,x,c,v,b` |
| `plugin:hyprexpo:selection_label_position` | string | label anchor | `top-right` |
| `plugin:hyprexpo:selection_label_color` | color | selection-token text color | `rgb(ffcc66)` |

## Window Icons

Draws each window's app icon over its spot in the workspace tile. The icon comes from the window's `.desktop` entry (file name or `StartupWMClass` matching the window class; Chromium `--app` windows match by the URL in `Exec`), falling back to the entry of the process that owns the window. Icons are looked up in the GTK icon theme, `hicolor`, then `pixmaps`. The lookup index is built on first use and rebuilt on config reload, so a newly installed app or icon shows up after `hyprctl reload`. Windows that only partly fit in a tile get their icon in the visible part; windows outside the tile get none.

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:window_icon_enable` | bool int | draw app icons on window previews | `0` |
| `plugin:hyprexpo:window_icon_position` | string | `center`, `top-left`, `top-right`, `bottom-left`, or `bottom-right` | `center` |
| `plugin:hyprexpo:window_icon_size` | int | maximum icon size in logical px | `128` |
| `plugin:hyprexpo:window_icon_size_ratio` | float | icon size as a share of the preview's shorter side, capped by `window_icon_size` | `0.3` |
| `plugin:hyprexpo:window_icon_offset_x` | int | inward offset from the anchor | `0` |
| `plugin:hyprexpo:window_icon_offset_y` | int | inward offset from the anchor | `0` |
| `plugin:hyprexpo:window_icon_bg_enable` | bool int | draw a background behind icons | `1` |
| `plugin:hyprexpo:window_icon_bg_color` | color | icon background color | `rgba(00000099)` |
| `plugin:hyprexpo:window_icon_bg_rounding` | int | icon background corner radius | `16` |
| `plugin:hyprexpo:window_icon_padding` | int | padding between icon and background edge | `8` |
