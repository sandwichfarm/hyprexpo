# HyprExpo

HyprExpo is a maintained Hyprland plugin that opens an expose-style workspace overview for current Hyprland releases. It keeps the original HyprExpo idea alive with keyboard navigation, visible labels, configurable grid styling, multi-monitor workspace placement, and Lua-configured gestures.

The plugin runs inside Hyprland, so reliability matters more than novelty. These docs prioritize installation, exact configuration shape, migration from legacy keyword examples, and safe runtime verification.

## Start Here

- [Install HyprExpo](./getting-started/installation)
- [Add a quick config](./getting-started/quick-start)
- [Review every option](./configuration/options)
- [Migrate older configs](./guides/migration)
- [Run the runtime smoke checklist](./guides/runtime-smoke)

## Current Public Surface

HyprExpo uses modern `plugin:hyprexpo:*` configuration keys plus Lua helpers under `hl.plugin.hyprexpo`. Legacy custom keywords such as `hyprexpo_gesture` and `hyprexpo_workspace_method` are migration inputs, not the active runtime surface.

## Runtime Boundary

HyprExpo is a shared object loaded by Hyprland. Build it against the Hyprland revision you run, and use `install` or `make install` when replacing a loaded plugin file so Hyprland does not keep a corrupted mapping.
