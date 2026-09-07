# Reproducing the Nix-Assisted hyprpm Validation

This is Nix-assisted hyprpm validation, not a generic Arch installation claim.
The proof builds the recorded plugin candidate without patching the upstream
manager, generated headers, version hashes, or ABI checks.

## Inputs

- Plugin: 330e071a19798b6202cdebab76ae5a4f201c8a72 from origin/integration/chase-validation.
- Hyprland: 34eb03bd8da01024596c367fba66485a8c9b8ca7.
- Dependencies: plugin flake.lock plus the matching flake development shell.
- Compositor: same Nix flake package, running privately under VirGL in QEMU.

## Environment preparation

Install standard guest build prerequisites: build-essential cmake cpio pkg-config acl git.
Evaluate the exact plugin flake development shell with its convenience shellHook disabled
(the hook only configures a local Meson development directory):

```sh
nix print-dev-env --impure --expr 'let f = builtins.getFlake "github:sandwichfarm/hyprexpo/330e071a19798b6202cdebab76ae5a4f201c8a72"; in f.devShells.x86_64-linux.default.overrideAttrs (_: {shellHook="";})' > consumer-env.sh
source consumer-env.sh
nix build github:sandwichfarm/hyprexpo/330e071a19798b6202cdebab76ae5a4f201c8a72#hyprland --no-link --json > compositor-build.json
export PATH="$(python3 -c 'import json; print(json.load(open("compositor-build.json"))[0]["outputs"]["out"])')/bin:$PATH"
```

Use matching Hyprland/bin first on PATH. Build its exposed `hyprland` output
from the same flake and record its store path. Scope HYPRLAND_INSTANCE_SIGNATURE
and XDG_RUNTIME_DIR to the disposable compositor explicitly.

Hyprpm invokes installheaders through sudo, which drops the Nix development PATH.
In this disposable guest only, expose the exact `command -v hyprwayland-scanner`
and `command -v wayland-scanner` targets in /usr/local/bin. Do not replace existing
system scanners outside an isolated validation VM. Standard make/cmake/cpio must
also exist in the privileged PATH.

In a guest where those destinations do not already exist:

```sh
sudo ln -s "$(command -v hyprwayland-scanner)" /usr/local/bin/hyprwayland-scanner
sudo ln -s "$(command -v wayland-scanner)" /usr/local/bin/wayland-scanner
```

The current manager creates root-owned build progress directories, then deletes
them as the original user. Set a default ACL on its PRIVATE disposable build root
before header preparation so the user can clean those children:

```sh
mkdir -p "$XDG_RUNTIME_DIR/hyprpm"
setfacl -m "d:u:$(id -un):rwx" "$XDG_RUNTIME_DIR/hyprpm"
```

Hyprpm hardcodes `pkgconf` for header validation. Ubuntu pkgconf traverses private
Nix dependencies not exposed by the upstream development shell. The small
`bin/pkgconf` adapter delegates to the ACTUAL Nix pkg-config wrapper and translates
only --keep-system-cflags / --keep-system-libs into their documented pkg-config
environment equivalents. All other args are forwarded unchanged; unsupported
queries fail in the real tool. It fabricates no flags, version or ABI results.
Use that adapter first on PATH for this proof. `manager-pkgconf-cflags.txt` records
the resolved flags pointing to hyprpm's genuinely generated header cache.

Portable adapter setup after entering the locked shell:

```sh
export HYPREXPO_REAL_PKG_CONFIG="$(command -v pkg-config)"
mkdir -p consumer-bin
cat > consumer-bin/pkgconf <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
args=()
for arg in "$@"; do
    case "$arg" in
        --keep-system-cflags) export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1 ;;
        --keep-system-libs) export PKG_CONFIG_ALLOW_SYSTEM_LIBS=1 ;;
        *) args+=("$arg") ;;
    esac
done
exec "${HYPREXPO_REAL_PKG_CONFIG:?}" "${args[@]}"
EOF
chmod +x consumer-bin/pkgconf
export PATH="$PWD/consumer-bin:$PATH"
```

## Actual manager operations

```sh
hyprpm --no-nix -v update
hyprpm --no-nix -v add https://github.com/sandwichfarm/hyprexpo origin/integration/chase-validation
hyprpm --no-nix -v enable hyprexpo
hyprpm --no-nix -v reload
```

--no-nix prevents a second nested shell because the exact locked build shell is
already active. Preserve full command logs and check per-plugin failure state,
artifact hashes, selected commit, ABI and loaded runtime behavior. A successful
process status alone is insufficient.

The header installation diagnostic tee wrapper used to capture the original
sudo failure was removed before installation of the plugin candidate.

The adapter's header query output was byte-compared with the direct real-tool
query. An unsupported option was checked to return failure. No version, header,
compiler flag, or ABI result was synthesized.

## Proof boundaries

The tested path performs genuine header preparation, repository selection,
compilation, cache installation, enabling, loading, and revision-specific update
through the unmodified manager. Loaded version and process memory mappings
identify the cache artifact, avoiding confusion with a previously loaded Nix
plugin that has the same name. Pointer and keyboard selection, scrolling
cancellation, exact submap restoration, and pinned-window preservation were
exercised with that artifact.

This environment recipe is for reproducing the validation VM. For an ordinary
Nix-managed desktop, prefer the [direct Nix consumer](../guides/development-installation.md#nix-consumer).
For a native distro hyprpm installation, provide that distro's matching headers
and complete development dependencies; do not copy these VM-only tool or ACL
adjustments into an unrelated host installation.
