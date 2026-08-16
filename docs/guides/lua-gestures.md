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

## Loading a Local Build

If you load a locally built `hyprexpo.so` directly from a Lua config, declare
its path on every config pass:

```lua
local plugin_path = "/absolute/path/to/hyprexpo.so"

-- This declares the desired plugin; it does not load the library immediately.
hl.plugin.load(plugin_path)

local hyprexpo_loaded = false
for _, plugin in ipairs(hl.get_loaded_plugins()) do
    if plugin.name == "hyprexpo" then
        hyprexpo_loaded = true
        break
    end
end

if hyprexpo_loaded then
    -- Put hl.config(), bindings, and immediate hl.plugin.hyprexpo calls here.
end
```

`hl.plugin.load()` is declarative. Hyprland loads the plugin after the first
config pass and reloads the config so its options and Lua functions are
available. Do not wrap `hl.plugin.load()` itself in the loaded-plugin check.
Omitting the path on the second pass tells Hyprland that the config no longer
wants the plugin, which can cause a repeating load/reload/unload cycle.

This is only needed for a local build loaded from the Lua config. `hyprpm`
manages its enabled plugin separately.

## Plugin Configuration

Configure HyprExpo options with `hl.config()` and a nested `plugin.hyprexpo`
table:

```lua
hl.config({
    plugin = {
        hyprexpo = {
            columns = 3,
            gaps_in = 5,
            gaps_out = 0,
            bg_col = "rgb(111111)",
            workspace_method = "center current",
            gesture_distance = 200,
            cancel_key = "escape",
            show_cursor = 1,
        },
    },
})
```

The `hl.plugin.hyprexpo` table shown above is for helper functions such as
`expo`, `kb_focus`, and `gesture`. Calling it as a function, or passing the
configuration table to `hl.plugin.hyprexpo.expo`, will raise a Lua type error.

## Keyboard Submap

Lua configs define submaps with Hyprland's Lua API. If you want modifierless
navigation while the overview is open, define the `hyprexpo` submap in
`hyprland.lua`:

```lua
hl.define_submap("hyprexpo", function()
    hl.bind("h",      function() hl.plugin.hyprexpo.kb_focus("left") end)
    hl.bind("l",      function() hl.plugin.hyprexpo.kb_focus("right") end)
    hl.bind("k",      function() hl.plugin.hyprexpo.kb_focus("up") end)
    hl.bind("j",      function() hl.plugin.hyprexpo.kb_focus("down") end)
    hl.bind("return", function() hl.plugin.hyprexpo.kb_confirm() end)
    hl.bind("escape", function() hl.plugin.hyprexpo.expo("cancel") end)
end)
```

HyprExpo still enters the submap named `hyprexpo` automatically when
`plugin:hyprexpo:keynav_enable` is enabled. The important part for Lua users is
that the submap's binds are registered with `hl.define_submap`, not with a
hyprlang `submap = hyprexpo` block.

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
