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

Append `all` to open on every monitor at once instead of only the one under the
cursor. Each monitor renders its own grid, honoring per-monitor
[workspace placement](../guides/multi-monitor).

```ini
bind = SUPER, g, hyprexpo:expo, toggle all
```

| option | description |
| --- | --- |
| `all` | shorthand for `toggle all` |
| `toggle all` | show the overview on every monitor if hidden, hide it everywhere if shown |
| `on all` or `enable all` | show the overview on every monitor; if some are already open, fills any missing monitor overviews |

The qualifier only affects opening. `off`, `cancel` and `select` always apply to
every open overview, so a single bind closes them all. Selecting a workspace
applies only on the monitor you acted on; the others dismiss without changing
their workspace.

`on all` and `enable all` are idempotent. Repeating either command keeps the
existing entries and fills any missing monitor overviews. `toggle all` keeps its
toggle behavior: if any overview is open, it closes every open overview instead
of filling the missing monitors. Every successful keyboard, pointer, or touch
selection also closes every open overview; only the overview that owns the
selected tile changes workspace.

Pointer-driven `select` and `bring` always act on the overview for the monitor under the pointer.
This remains true after keyboard focus has crossed to another
monitor: `bring` moves the chosen window into the pointer monitor's active
workspace rather than the keyboard-owned or compositor-focused monitor.

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
