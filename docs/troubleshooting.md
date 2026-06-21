# Troubleshooting

## Plugin Load Fails With API or Hash Mismatch

Rebuild HyprExpo against the same Hyprland revision that is running, then reload the plugin.

## Plugin Load Fails Because Dependencies Are Missing

Install the build dependencies and rebuild. Current linked runtime dependencies include Lua (`lua5.4` or `lua` through pkg-config), `pangocairo`, and `xkbcommon`.

## Invalid Config Values

Invalid `workspace_method` or border color values should be logged and fall back safely. Use the documented formats in [configuration options](./configuration/options) and [multi-monitor layouts](./guides/multi-monitor).

## Columns Outside the Supported Range

`columns` is clamped to `1..7` so pointer and keyboard selection stay in bounds.

## Replacing a Loaded Plugin Crashes Hyprland

For development, prefer `./scripts/run-nested.sh` or `make dev-reload` so Hyprland loads a fresh user-owned build by absolute path. If you intentionally replace an installed plugin file, use `make install` or `install` instead of overwriting the file with `cp`.

## Per-Monitor Placement Does Not Apply

Check the monitor name from Hyprland and use comma-separated entries such as:

```ini
plugin {
    hyprexpo {
        workspace_method = DP-1 first 1, HDMI-1 center 5, center current
    }
}
```
