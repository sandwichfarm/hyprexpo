<div align="center">

# HyprExpo

**An Exposé-style workspace and window overview for Hyprland.**

[![Release](https://img.shields.io/github/v/release/sandwichfarm/hyprexpo?style=for-the-badge)](https://github.com/sandwichfarm/hyprexpo/releases)
[![Compatibility checks](https://img.shields.io/github/actions/workflow/status/sandwichfarm/hyprexpo/compatibility.yml?branch=master&style=for-the-badge&label=compatibility)](https://github.com/sandwichfarm/hyprexpo/actions/workflows/compatibility.yml)
[![BSD 3-Clause license](https://img.shields.io/github/license/sandwichfarm/hyprexpo?style=for-the-badge)](LICENSE)

[Quick start](#quick-start) · [Documentation](https://hyprexpo.lol/docs/) · [Configuration](#quick-config) · [Troubleshooting](docs/troubleshooting.md)

</div>

https://github.com/user-attachments/assets/861baa26-46b6-4fa8-8d37-65cbb9ecbed4

## What is this?

HyprExpo is a maintained Hyprland plugin that lets you inspect workspaces, select
one with the pointer or keyboard, and move windows by dragging their previews.
It continues the original HyprExpo plugin with configurable grids, labels,
borders, multi-monitor placement, and Lua gestures.

- **Workspace grids:** square, rectangular, or dynamic layouts, with workspace
  names, configurable gaps, rounded tiles, and borders.
- **Keyboard and gestures:** focus navigation, workspace or visible-position
  selection, cancellation, and Lua-configured touchpad gestures.
- **Multi-monitor placement:** choose which workspaces appear on each monitor.
- **Native scrolling overview:** inspect individual windows, including offscreen
  windows, pan through the layout, and move windows within or between workspaces.
  Mixed-layout rows reuse workspace previews.

The [scrolling overview](docs/guides/scrolling-overview.md) supports pointer,
touch, and keyboard input. Its scope excludes hot corners, dwell activation,
layer-shell/wallpaper composition, and arbitrary workspace insertion.

## Quick Start

### 1. Check compatibility

`master` supports tagged Hyprland **v0.56.1 and v0.56.2**. The plugin must be
built against the **exact revision and dependency ABI** of your running
compositor; a binary built for one release is not interchangeable with another.

```bash
hyprctl version
```

This development checkout pins Hyprland
`663b29b846e4bad17cfa29941f6d2fbb6b61f139`. For Hyprland-git, use the separate [development installation guide](docs/guides/development-installation.md).
For Nix-managed Hyprland, use the [Nix installation path](#nix).

<a id="install"></a>
<a id="hyprpm"></a>

### 2. Install with hyprpm

```bash
hyprpm add https://github.com/sandwichfarm/hyprexpo
hyprpm enable hyprexpo
hyprpm reload
```

The repository and plugin are both named `hyprexpo`; the build output is
`hyprexpo.so`. On supported releases, hyprpm uses the compatible commits listed
in [`hyprpm.toml`](hyprpm.toml). Changes on `master` do not automatically move
those pins.

### 3. Bind and open the overview

Add the binding for your active config format. In `hyprland.conf`:

```ini
bind = SUPER, g, hyprexpo:expo, toggle
```

Or in `hyprland.lua`:

```lua
hl.bind("SUPER + G", function()
    hl.plugin.hyprexpo.expo("toggle")
end)
```

Reload your configuration, then press **Super + G** to open the overview. Click a
workspace preview to select it; press **Escape** to cancel. For arrow-key or
Vim-style navigation, add the [keyboard submap bindings](docs/configuration/keyboard.md)
for your config format.

To confirm the plugin loaded and check for configuration errors:

```bash
hyprctl plugin list
hyprctl configerrors
```

See [troubleshooting](docs/troubleshooting.md) if loading or configuration fails.

## Quick Config

The defaults work without a plugin block. To customize the workspace grid, add
this to `hyprland.conf`:

```ini
plugin {
    hyprexpo {
        columns = 3
        rows = 0 # Follow columns for a square grid.
        gaps_in = 5
        gaps_out = 0
        bg_col = rgb(111111)
        workspace_method = center current
    }
}
```

For `hyprland.lua`, use `hl.config()` instead:

```lua
hl.config({
    plugin = {
        hyprexpo = {
            columns = 3,
            rows = 0, -- Follow columns for a square grid.
            gaps_in = 5,
            gaps_out = 0,
            bg_col = "rgb(111111)",
            workspace_method = "center current",
        },
    },
})
```

- **Ten fixed slots:** set `columns = 5`, `rows = 2`, `dynamic_grid = 0`, and
  `skip_empty = 0`. Empty workspaces remain selectable and accept dragged windows.
  Positive `rows` values are clamped to `1..7`; dynamic grids and native scrolling
  overviews ignore this option.
- **Click without moving windows:** set `drag_drop_enable = 0` to disable dragging
  workspace previews. Drag and drop is enabled by default.
- **Pinned/PiP previews:** `show_pinned_windows = 0` hides them from thumbnails
  by default, while leaving their normal Hyprland behavior unchanged.
- **Number keys:** `number_key_mode = workspace` selects global workspace IDs;
  `index` selects visible positions; `passthrough` leaves digits to your own binds.

### Active workspace grid

For a dynamic grid with workspace labels and a wallpaper background:

```ini
plugin {
    hyprexpo {
        dynamic_grid = 1
        fill_gaps = 0
        mru_sort = 0
        show_workspace_names = 1
        label_pos = top_right
        label_size = 48
        wallpaper_bg = 1
    }
}
```

In Lua, put these options inside the `plugin.hyprexpo` table passed to
`hl.config()` above. See [all configuration options](docs/configuration/options.md),
[labels and borders](docs/configuration/labels-borders.md), and
[Lua gestures](docs/guides/lua-gestures.md) for complete examples.

## Other Installation Methods

### Build From Source

Install a C++23 compiler, `pkg-config`, Hyprland development headers matching your
compositor, and these pkg-config packages:

```text
hyprland pixman-1 libdrm pangocairo libinput libudev wayland-server xkbcommon lua5.4
```

The build prefers `lua5.4` and falls back to `lua` on distributions that expose
the generic module name.

```bash
git clone https://github.com/sandwichfarm/hyprexpo
cd hyprexpo
make all
```

Test local builds in a disposable nested session with `./scripts/run-nested.sh`.
Use `make dev-load` and `make dev-reload` from an existing disposable session.
See the [installation guide](docs/getting-started/installation.md) for Meson,
CMake, and managed installation commands, and follow the
[local installation contract](CONTRIBUTING.md#local-installation-contract).

Do not overwrite a loaded `.so` with `cp`: replacing its contents in place can
corrupt Hyprland's live memory mapping. Keep PR commits and temporary branches
out of a desktop's saved hyprpm revision; use the development load commands for
PR testing.

### Nix

Use the repository's [`flake.nix`](flake.nix) or [`default.nix`](default.nix)
through the Nix Hyprland plugin path. Keep the plugin and compositor on the same
Hyprland input; do not mix a hyprpm artifact into a Nix-managed session.

The `master` flake defaults to Hyprland v0.56.2. This development checkout pins
the upstream commit above. Input overrides must select a revision supported by
the plugin source; a release input cannot make development APIs compatible. See [Nix installation](docs/getting-started/installation.md#nix)
and the [aligned-input examples](docs/guides/development-installation.md#nix-consumer).

## Branches and Releases

| Track | Compatibility contract |
| --- | --- |
| `master` | Supported tagged Hyprland releases: v0.56.1 and v0.56.2. |
| `hyprland-git` | Explicitly tested upstream development commits; check its current pin before installing. |

Shared fixes flow from `master` into `hyprland-git`. Support for a new Hyprland
release returns through a validated promotion PR. See the
[branch and release policy](docs/reference/branch-policy.md) and
[compatibility and release provenance](docs/reference/compatibility.md).

<a id="next-steps"></a>

## Documentation

Browse the [documentation website](https://hyprexpo.lol/docs/) or the
[Markdown index](docs/index.md).

| Guide | Use it to… |
| --- | --- |
| [Installation](docs/getting-started/installation.md) | Install with hyprpm, build from source, or use Nix. |
| [Quick start](docs/getting-started/quick-start.md) | Configure an overview and its keyboard bindings. |
| [Configuration options](docs/configuration/options.md) | Look up defaults and layout settings. |
| [Keyboard navigation](docs/configuration/keyboard.md) | Set up submaps, number keys, and cancellation. |
| [Dispatchers](docs/reference/dispatchers.md) | Control the overview from binds and Lua. |
| [Multi-monitor placement](docs/guides/multi-monitor.md) | Choose workspace placement per monitor. |
| [Scrolling overview](docs/guides/scrolling-overview.md) | Use window previews, panning, and positional moves. |
| [Migration](docs/guides/migration.md) | Replace old keyword configuration. |
| [Troubleshooting](docs/troubleshooting.md) | Diagnose loading, configuration, and saved-revision errors. |
| [Chasing Hyprland](docs/guides/chasing-hyprland.md) | Prepare a bounded development compatibility candidate. |
| [Runtime smoke checklist](docs/guides/runtime-smoke.md) | Check loading, interaction, and teardown. |

## Project Structure

```text
hyprexpo/
├── .github/workflows/  # Compatibility, release, and site CI
├── docs/              # User guides and reference documentation
├── scripts/           # Build, development, validation, and release helpers
├── site/              # Website sources
├── src/               # Plugin implementation and private headers
├── tests/             # C++ regression suites and Python tooling tests
├── Makefile           # Primary build; produces hyprexpo.so
├── flake.nix          # Nix plugin and matching compositor packages
└── hyprpm.toml        # Plugin metadata and release compatibility pins
```

See [repository layout](docs/reference/repository-layout.md) for the full build
and directory conventions.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before changing compatibility or a managed
installation. Include the Hyprland revision you tested and the relevant
[smoke-check results](docs/guides/runtime-smoke.md) with your change.

For bug reports, [open an issue](https://github.com/sandwichfarm/hyprexpo/issues/new)
with steps to reproduce, `hyprctl version`, your installation method, relevant
plugin configuration, and any errors.

[![HyprExpo contributors](https://contrib.rocks/image?repo=sandwichfarm/hyprexpo)](https://github.com/sandwichfarm/hyprexpo/graphs/contributors)

## History

This fork began with [additions to the original HyprExpo](https://github.com/hyprwm/hyprland-plugins/pull/507)
and was previously known as **HyprExpo+** (`hyprexpo-plus`). It continues
maintenance after [the upstream plugin was retired](https://github.com/hyprwm/hyprland-plugins/pull/663).
See the [original announcement](https://www.reddit.com/r/hyprland/comments/1o30dsg/hyprexpoplus_outer_gaps_keyboard_navigation_and/)
for the early feature set.

### Related

[colonelpanic8/hyprexpo](https://github.com/colonelpanic8/hyprexpo) is another
HyprExpo fork.

## License

[BSD 3-Clause](LICENSE), with the original Hypr Development copyright retained.

<details>
<summary>Star history</summary>

[![Star history](https://api.star-history.com/svg?repos=sandwichfarm/hyprexpo&type=Date)](https://star-history.com/#sandwichfarm/hyprexpo&Date)

</details>
