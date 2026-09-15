# Troubleshooting

## hyprpm Cannot Check Out a Saved Revision

If `hyprpm update` prints `Plugin has revision set, resetting` followed by
`Could not parse object`, inspect `repository.rev` in
`/var/cache/hyprpm/$USER/hyprexpo/state.toml`. An explicit revision overrides
the repository's compatibility pins. A PR commit can disappear from normal
clones after a squash merge and deletion of its temporary branch; retaining
the object in a developer checkout does not make the installation updateable.

From a source checkout, run `make check-hyprpm-state`. The check makes no changes.
It accepts unpinned state offline, and verifies explicit revisions using a fresh
clone. Failed network access also refuses replacement instead of assuming the
revision is safe. It cannot establish ABI compatibility or guarantee that an
upstream maintainer will preserve the accepted refs in the future.

To restore normal pin selection on a **supported released Hyprland**, first
back up the installed state and binary, then re-register without a revision:

```bash
backup_root="${XDG_STATE_HOME:-$HOME/.local/state}/hyprexpo"
mkdir -p "$backup_root" &&
backup_dir="$(mktemp -d "$backup_root/revision-recovery.XXXXXX")" &&
cp -a "/var/cache/hyprpm/$USER/hyprexpo" "$backup_dir/" &&
hyprpm remove hyprexpo &&
./scripts/hyprpm-add.sh https://github.com/sandwichfarm/hyprexpo &&
hyprpm enable hyprexpo &&
hyprpm reload &&
hyprctl reload
```

Keep the backup until loading succeeds. Run hyprpm as your normal user and
authenticate its sudo prompt in your terminal. If a command fails, stop and
retain the output and backup. Do not delete the entire cache or replace `rev`
with another temporary PR hash. Development-track installations must retain
their matching compositor and use the [development recovery procedure](./guides/development-installation.md#troubleshooting-and-rollback).

Verify `hyprctl version`, `hyprctl plugin list`, and `hyprctl configerrors` after
recovery. A failed install can unload the plugin and cause `unknown config key
'plugin.hyprexpo.…'` errors until it is loaded again. Finish loading and reload
the config before treating these as removed options.

## Plugin Load Fails With API or Hash Mismatch

Rebuild HyprExpo against the same Hyprland revision that is running, then load
the rebuilt plugin explicitly and reload the config:

```
hyprctl plugin load /var/cache/hyprpm/$USER/hyprexpo/hyprexpo.so   # hyprpm install
hyprctl plugin load ~/.local/lib/hyprland-plugins/hyprexpo.so      # local install
hyprctl reload
```

A plain `hyprctl reload` does not retry a config-declared load that already
failed earlier in the session. Hyprland only re-attempts config-managed plugin
loads when the declared plugin list changes, so a failed attempt stays sticky
until the next session. Loading the rebuilt plugin manually clears the failure
immediately, and the following `hyprctl reload` applies the plugin options and
binds again.

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
