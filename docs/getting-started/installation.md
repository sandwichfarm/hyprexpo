# Installation

## hyprpm

hyprpm is the primary non-Nix installation path.

```bash
hyprpm add https://github.com/sandwichfarm/hyprexpo-plus
hyprpm enable hyprexpo
hyprpm reload
```

The plugin repository is named `hyprexpo` in `hyprpm.toml`, and the built output is `hyprexpo.so`.

## Build From Source

Build dependencies are a C++23 compiler, `pkg-config`, Hyprland development headers, and these pkg-config packages:

```text
hyprland pixman-1 libdrm pangocairo libinput libudev wayland-server xkbcommon lua5.4
```

Build with the Makefile:

```bash
git clone https://github.com/sandwichfarm/hyprexpo-plus
cd hyprexpo-plus
make all
```

Alternative build entry points are available:

```bash
meson setup build
meson compile -C build
```

```bash
cmake -S . -B build
cmake --build build
```

## Install a Local Build

Install the Makefile build over the hyprpm-managed copy:

```bash
make install
hyprpm reload
```

Equivalent manual install:

```bash
install -Dm755 hyprexpo.so \
    /var/cache/hyprpm/$USER/hyprexpo/hyprexpo.so
hyprpm reload
```

::: warning
Use `install`, not plain `cp`, when replacing a loaded `.so`. Hyprland maps the plugin into the running process. Overwriting that file in place can corrupt the live mapping and crash the session.
:::

## Nix

Nix users should build HyprExpo through the Nix Hyprland plugin path instead of mixing a `hyprpm` artifact into a Nix-managed Hyprland session. The repository contains `default.nix`, which uses `hyprlandPlugins.mkHyprlandPlugin` so the plugin follows the Hyprland input supplied by the caller.

Hyprland plugins are ABI-sensitive. Keep the plugin build and running Hyprland revision aligned. If Hyprland is updated, rebuild HyprExpo from the same Hyprland input before loading it.
