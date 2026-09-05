# Multi-Monitor Workspace Placement

`plugin:hyprexpo:workspace_method` can be global, per-monitor, or a mix of per-monitor entries with a global fallback.

## Formats

```text
center <workspace>
first <workspace>
MONITOR center <workspace>
MONITOR first <workspace>
```

Separate multiple entries with commas:

```ini
plugin {
    hyprexpo {
        workspace_method = DP-1 first 1, HDMI-1 center 5, eDP-1 first 10
    }
}
```

Mixed monitor-specific entries and fallback:

```ini
plugin {
    hyprexpo {
        workspace_method = DP-1 first 1, center current
    }
}
```

## Active workspace beyond the grid

`first <workspace>` anchors the grid to a fixed workspace and counts upward, so
the configured `columns` normally bound how many workspaces are visible. When
the currently active workspace sits past the last tile (for example `first 1`
with `columns = 3` shows workspaces 1–9, but the active workspace is 10), the
overview temporarily grows the grid so the active workspace stays visible and
the open/close animation focuses on it instead of the anchor tile. The grid only
grows — never below the configured `columns` — and is capped at the maximum of 7
columns. This applies to plain sequential grids (not `skip_empty` or
`max_workspace`, which keep their explicit bounds).

## Opening on Every Monitor

By default the overview opens only on the monitor under the cursor. Append `all`
to the dispatcher argument to open one overview per monitor instead:

```ini
bind = SUPER, g, hyprexpo:expo, toggle all
```

Each monitor builds its own grid from its own anchor, so this composes with
per-monitor `workspace_method`. With the placement below, the overview on
`DP-1` shows workspaces 1-4 and the one on `HDMI-1` shows 5-8:

```ini
plugin {
    hyprexpo {
        columns = 2
        max_workspace = 8
        workspace_method = DP-1 first 1, HDMI-1 first 5
    }
}
```

Keyboard navigation has one explicit keyboard owner. Movement stays inside that
overview while a valid local tile (including a configured wrap target) exists.
At an exhausted edge, the plugin uses global logical geometry to choose the
nearest tile in that direction on another monitor. Selecting it switches only
the target monitor, then dismisses every open overview.

Window previews can also be dragged between monitor overviews. The monitor under
the pointer renders the proxy with the target monitor's own logical tile layout
and scale. After a valid drop, source and target thumbnails refresh
independently. Releasing over a gap, the source tile, or an invalid target moves
nothing; cleanup still removes highlights, restores the cursor, and repaints
every monitor visited by the drag.

## Troubleshooting Monitor Names

If a per-monitor entry does not apply, check the monitor name reported by Hyprland and use that exact name in the comma-separated list.

Invalid values should fall back safely instead of crashing the compositor.
