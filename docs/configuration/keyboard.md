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
