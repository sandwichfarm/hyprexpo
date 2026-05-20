# HyprExpo

HyprExpo is a maintained Hyprland plugin for expose-style workspace overview with keyboard selection, labels, configurable gaps and borders, multi-monitor placement, and Lua gestures.

Docs: [docs/index.md](docs/index.md)

Announcement: [r/hyprland launch post](https://www.reddit.com/r/hyprland/comments/1o30dsg/hyprexpoplus_outer_gaps_keyboard_navigation_and/)

## Install

### hyprpm

```bash
hyprpm add https://github.com/sandwichfarm/hyprexpo
hyprpm enable hyprexpo
hyprpm reload
```

The repository name in `hyprpm.toml` is `hyprexpo`, and the built plugin output is `hyprexpo.so`.

### Build From Source

Install a C++23 compiler, `pkg-config`, Hyprland development headers, and these pkg-config packages:

```text
hyprland pixman-1 libdrm pangocairo libinput libudev wayland-server xkbcommon lua5.4
```

Build with the Makefile:

```bash
git clone https://github.com/sandwichfarm/hyprexpo
cd hyprexpo
make all
```

Install the local build over the hyprpm-managed copy:

```bash
make install
hyprpm reload
```

Use `install` or `make install`, not plain `cp`, when replacing a loaded `.so`. Hyprland maps plugin files into the running process, and overwriting that file in place can corrupt the live mapping.

Other build entry points:

```bash
meson setup build
meson compile -C build
```

```bash
cmake -S . -B build
cmake --build build
```

### Nix

Nix users should build HyprExpo through the Nix Hyprland plugin path instead of mixing a `hyprpm` artifact into a Nix-managed Hyprland session. This repository includes `default.nix`, which uses `hyprlandPlugins.mkHyprlandPlugin` so the plugin follows the Hyprland input supplied by the caller.

Hyprland plugins are ABI-sensitive. Keep the plugin build and running Hyprland revision aligned.

## Quick Config

Add the plugin block to your Hyprland config:

```ini
plugin {
    hyprexpo {
        columns = 3
        gaps_in = 5
        gaps_out = 0
        bg_col = rgb(111111)
        workspace_method = center current
        gesture_distance = 200
        cancel_key = escape
        show_cursor = 1
    }
}
```

Add a dispatcher binding:

```ini
bind = SUPER, g, hyprexpo:expo, toggle
```

Optional keyboard navigation:

```ini
plugin {
    hyprexpo {
        keynav_enable = 1
        keynav_wrap_h = 1
        keynav_wrap_v = 1
        keynav_reading_order = 0
    }
}

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

## Next Steps

- [Installation details](docs/getting-started/installation.md)
- [Quick start](docs/getting-started/quick-start.md)
- [All configuration options](docs/configuration/options.md)
- [Labels and borders](docs/configuration/labels-borders.md)
- [Keyboard navigation](docs/configuration/keyboard.md)
- [Lua gestures](docs/guides/lua-gestures.md)
- [Multi-monitor placement](docs/guides/multi-monitor.md)
- [Migration from old keyword config](docs/guides/migration.md)
- [Runtime smoke checklist](docs/guides/runtime-smoke.md)
- [Compatibility and release provenance](docs/reference/compatibility.md)
- [Dispatcher reference](docs/reference/dispatchers.md)
- [Troubleshooting](docs/troubleshooting.md)
