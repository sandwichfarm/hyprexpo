# Compatibility and Release Provenance

Hyprland checks plugin API compatibility when loading a plugin. If HyprExpo was built against an incompatible Hyprland revision, loading should fail with a visible API/hash mismatch instead of silently running against the wrong ABI.

## Build Provenance

Release builds attach `release-provenance.txt` next to `hyprexpo.so`. That file records:

- `hyprctl version`
- `pkg-config --modversion hyprland`
- `pkg-config --modversion lua5.4`
- compiler version
- `ldd -r hyprexpo.so`

## Supported Target

HyprExpo should be built against the Hyprland revision that will load it. Nix users should build through the Nix plugin path so the plugin follows the Hyprland input supplied by the caller.

## Current Known Local Verification

The v1 planning state recorded automated verification against Hyprland `0.55.1` with ABI string:

```text
a47147bc095e5b3be3eb8bd04f0ac242b968cd4d_aq_0.11_hu_0.13_hg_0.5_hc_0.1_hlg_0.6
```

That evidence is local verification context, not a blanket compatibility promise for every Hyprland git revision.
