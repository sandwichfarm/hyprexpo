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
