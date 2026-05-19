# Migration From Legacy Keywords

Legacy `hyprexpo_workspace_method` and `hyprexpo_gesture` lines are no longer registered on Hyprland 0.55+. Use the migration tool when updating older configs.

## Dry Run

```bash
python3 tools/hyprexpo-migrate-config.py ~/.config/hypr/hyprland.conf
```

Dry-run behavior:

- Does not modify config files.
- Prints the unified diff to stdout.
- Prints generated Lua gesture snippets and warnings to stderr.
- Ignores `--lua-out` unless `--write` is also set, so dry runs do not create files.

## Write Changes

```bash
python3 tools/hyprexpo-migrate-config.py \
    --write \
    --lua-out ~/.config/hypr/hyprexpo.lua \
    ~/.config/hypr/hyprland.conf
```

Write behavior:

- Rewrites each listed config file in place.
- Creates `*.bak` files before overwriting unless `--no-backup` is set.
- Writes generated Lua gesture snippets to `--lua-out` when provided.
- Backs up an existing `--lua-out` file before overwriting unless `--no-backup` is set.
- Only processes files named on the command line; it does not follow Hyprland `source = ...` includes.

## Example

Input:

```ini
hyprexpo_workspace_method = DP-1 first 1
hyprexpo_workspace_method = HDMI-A-1 center 5
hyprexpo_gesture = swipe:4:up, hyprexpo:expo, toggle
```

Migrated Hyprland config:

```ini
plugin {
    hyprexpo {
        workspace_method = DP-1 first 1, HDMI-A-1 center 5
    }
}
```

Generated Lua:

```lua
hl.plugin.hyprexpo.gesture({ fingers = 4, direction = "up", action = "expo" })
```

Gesture lines that cannot be parsed are commented with a manual-migration TODO instead of being deleted.
