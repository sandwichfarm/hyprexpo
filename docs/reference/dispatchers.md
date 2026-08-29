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

## Scrolling diagnostics

These dispatchers are for disposable acceptance sessions, not normal binds:

| dispatcher | argument | description |
| --- | --- | --- |
| `hyprexpo:scrolling_debug` | `REQUEST_ID active` or `REQUEST_ID workspace:ID` | emit one read-only, request-correlated native topology record |
| `hyprexpo:scrolling_input_test` | `REQUEST_ID|EVENT_SEQUENCE` | run strict deterministic input transitions when `scrolling_input_debug = 1` |
| `hyprexpo:scrolling_mutation_test` | `REQUEST_ID SOURCE_WORKSPACE TARGET_STABLE_ID KIND DESTINATION_WORKSPACE COLUMN ROW [FAULT]` | execute one loaded native acceptance transaction when `scrolling_input_debug = 1` |

Topology records are logged with `HYPREXPO_SCROLLING_DIAGNOSTIC`; input records
use `HYPREXPO_SCROLLING_INPUT`; positional releases use
`HYPREXPO_SCROLLING_MUTATION`. Request IDs accept only ASCII letters, digits,
dot, underscore, and dash and are capped at 64 characters. Diagnostics copy
native state and compare it before/after; they do not move the native tape,
focus, cursor, or windows. Mutation records include outcome, rollback status,
invariant IDs, and secret-free topology hashes.

The mutation acceptance dispatcher is intended only for the disposable nested
validator. `KIND` is one of `same-column`, `existing-column`,
`new-column-before`, `new-column-after`, `cross-scrolling`, `mixed-workspace`,
`terminal-workspace`, or `no-op-release`. Terminal requests use destination
workspace `0`, which is resolved to the next empty workspace at release time.
The sole accepted fault is `apply:add-target:after`; it is scoped to that one
request and proves rollback after a real native mutation.
