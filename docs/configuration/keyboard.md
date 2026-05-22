# Keyboard Navigation

Keyboard navigation is enabled by default. The overview can activate a `hyprexpo` submap while it is open, then reset the submap when the overview closes.

## Options

| key | type | description | default |
| --- | --- | --- | --- |
| `plugin:hyprexpo:keynav_enable` | bool int | enable keyboard navigation and the overview submap | `1` |
| `plugin:hyprexpo:keynav_wrap_h` | bool int | wrap horizontally at row edges | `1` |
| `plugin:hyprexpo:keynav_wrap_v` | bool int | wrap vertically at column edges | `1` |
| `plugin:hyprexpo:keynav_reading_order` | bool int | use row-major horizontal movement | `0` |

## Binds

Use this syntax in `hyprland.conf`:

```ini
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

Use this syntax in `hyprland.lua`:

```lua
hl.define_submap("hyprexpo", function()
    hl.bind("left",   function() hl.plugin.hyprexpo.kb_focus("left") end)
    hl.bind("right",  function() hl.plugin.hyprexpo.kb_focus("right") end)
    hl.bind("up",     function() hl.plugin.hyprexpo.kb_focus("up") end)
    hl.bind("down",   function() hl.plugin.hyprexpo.kb_focus("down") end)
    hl.bind("return", function() hl.plugin.hyprexpo.kb_confirm() end)
    hl.bind("escape", function() hl.plugin.hyprexpo.expo("cancel") end)
    hl.bind("1",      function() hl.plugin.hyprexpo.kb_selecti(1) end)
    hl.bind("2",      function() hl.plugin.hyprexpo.kb_selecti(2) end)
    hl.bind("3",      function() hl.plugin.hyprexpo.kb_selecti(3) end)
    hl.bind("4",      function() hl.plugin.hyprexpo.kb_selecti(4) end)
    hl.bind("5",      function() hl.plugin.hyprexpo.kb_selecti(5) end)
    hl.bind("6",      function() hl.plugin.hyprexpo.kb_selecti(6) end)
    hl.bind("7",      function() hl.plugin.hyprexpo.kb_selecti(7) end)
    hl.bind("8",      function() hl.plugin.hyprexpo.kb_selecti(8) end)
    hl.bind("9",      function() hl.plugin.hyprexpo.kb_selecti(9) end)
    hl.bind("0",      function() hl.plugin.hyprexpo.kb_selecti(10) end)
end)
```

The plugin enters the submap named `hyprexpo` when the overview opens and
resets it when the overview closes. The matching submap binds must be defined in
the active config system. On Hyprland 0.55+ Lua configs, that means
`hl.define_submap("hyprexpo", ...)`; a `submap = hyprexpo` block in
`hyprland.conf` is not loaded by a `hyprland.lua` setup.

For vim-style navigation, bind `h`, `j`, `k`, and `l` inside the same Lua
submap:

```lua
hl.define_submap("hyprexpo", function()
    hl.bind("h", function() hl.plugin.hyprexpo.kb_focus("left") end)
    hl.bind("l", function() hl.plugin.hyprexpo.kb_focus("right") end)
    hl.bind("k", function() hl.plugin.hyprexpo.kb_focus("up") end)
    hl.bind("j", function() hl.plugin.hyprexpo.kb_focus("down") end)
end)
```

## Cancel Behavior

`cancel_key` closes the overview without selecting a workspace. Use comma-separated key names for multiple cancel keys:

```ini
plugin {
    hyprexpo {
        cancel_key = escape, q
    }
}
```

Set `cancel_key = none` or `cancel_key = off` to disable key-based cancel matching.
