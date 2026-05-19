# Configuration Options

Normal settings live under `plugin:hyprexpo:*`, either in the plugin block or with fully qualified Hyprland config keys.

Hyprland 0.55 deprecated the custom keyword API that older HyprExpo configs used. HyprExpo no longer registers `hyprexpo_gesture` or `hyprexpo_workspace_method`. Use `plugin:hyprexpo:workspace_method` for workspace placement and the Lua API for gestures.

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

## Color Values

Color-backed settings such as `bg_col`, `label_color*`, `label_bg_color`, and `selection_label_color` use Hyprland color parsing. Solid border colors are strings because border settings also accept gradient specs.

Solid values:

```ini
border_color_current = rgb(66ccff)
```

Gradient values:

```ini
border_color_current = rgba(33ccffee) rgba(00ff99ee) 45deg
```

## Safe Failure Behavior

Invalid `columns`, workspace methods, label tokens, border colors, and gradient values are expected to fail safely. The plugin should log invalid values or use a fallback instead of crashing Hyprland during render.
