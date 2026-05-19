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

## Troubleshooting Monitor Names

If a per-monitor entry does not apply, check the monitor name reported by Hyprland and use that exact name in the comma-separated list.

Invalid values should fall back safely instead of crashing the compositor.
