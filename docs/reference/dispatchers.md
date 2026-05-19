# Dispatchers

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

Hyprland may briefly report invalid dispatcher messages during startup if binds are parsed before plugins are loaded. Those messages are cosmetic; the dispatchers work once the plugin is loaded.
