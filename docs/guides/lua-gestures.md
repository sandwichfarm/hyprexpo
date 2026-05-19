# Lua Gestures

HyprExpo exposes helpers under `hl.plugin.hyprexpo` when Hyprland is using Lua config support.

```lua
hl.plugin.hyprexpo.expo("toggle")
hl.plugin.hyprexpo.expo("cancel")
hl.plugin.hyprexpo.kb_focus("left")
hl.plugin.hyprexpo.kb_confirm()
hl.plugin.hyprexpo.kb_selecti(1)
hl.plugin.hyprexpo.kb_selectn(1)
hl.plugin.hyprexpo.kb_select("1")
```

## Gesture Setup

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

## Argument Validation

Lua arguments are validated strictly:

| function | accepted arguments |
| --- | --- |
| `expo` | string, defaulting to `"toggle"` when omitted |
| `kb_focus` | string |
| `kb_confirm` | none |
| `kb_selecti` | Lua integer or exact integer string |
| `kb_selectn` | Lua integer or exact integer string |
| `kb_select` | string |

Fractional numbers, booleans, tables where strings are expected, and partial numeric strings are rejected instead of being silently coerced.
