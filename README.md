# HyprExpo

HyprExpo is a maintained Hyprland plugin for expose-style workspace overview with keyboard selection, drag-drop window movement, labels, configurable gaps and borders, multi-monitor placement, and Lua gestures.

Docs: [docs/index.md](docs/index.md)

Announcement: [r/hyprland launch post](https://www.reddit.com/r/hyprland/comments/1o30dsg/hyprexpoplus_outer_gaps_keyboard_navigation_and/)

## History

HyprExpo continues the original expose-style workspace overview plugin from the
Hyprland plugins ecosystem. After the upstream plugin was retired from the
official plugin set, this fork kept the idea maintained for current Hyprland
releases instead of leaving users on stale plugin APIs.

The HyprExpo+ work also became the home for practical additions that made the
overview more usable day to day: keyboard navigation, visible workspace labels,
configurable gaps and borders, multi-monitor placement, and Lua gesture setup.
See the [upstream retirement context](https://github.com/hyprwm/hyprland-plugins/pull/507#issuecomment-4433386463)
and the [launch announcement](https://www.reddit.com/r/hyprland/comments/1o30dsg/hyprexpoplus_outer_gaps_keyboard_navigation_and/)
for the project background.

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

- [Installation details](https://hyprexpo.lol/docs/getting-started/installation/)
- [Quick start](https://hyprexpo.lol/docs/getting-started/quick-start/)
- [All configuration options](https://hyprexpo.lol/docs/configuration/options/)
- [Labels and borders](https://hyprexpo.lol/docs/configuration/labels-borders/)
- [Keyboard navigation](https://hyprexpo.lol/docs/configuration/keyboard/)
- [Lua gestures](https://hyprexpo.lol/docs/guides/lua-gestures/)
- [Multi-monitor placement](https://hyprexpo.lol/docs/guides/multi-monitor/)
- [Migration from old keyword config](https://hyprexpo.lol/docs/guides/migration/)
- [Runtime smoke checklist](https://hyprexpo.lol/docs/guides/runtime-smoke/)
- [Compatibility and release provenance](https://hyprexpo.lol/docs/reference/compatibility/)
- [Dispatcher reference](https://hyprexpo.lol/docs/reference/dispatchers/)
- [Troubleshooting](https://hyprexpo.lol/docs/troubleshooting/)
