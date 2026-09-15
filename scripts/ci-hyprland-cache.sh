#!/usr/bin/env bash
# Sourced by ci-build.sh after its complete flake.lock has been frozen.

configure_hyprland_cache() {
    local url=${BUNNY_CACHE_URL:-} key=${BUNNY_CACHE_PUBLIC_KEY:-}
    if [[ ${BUNNY_CACHE_WRITE:-false} == true ]]; then
        : "${BUNNY_CACHE_URL:?Cache writes require the public cache URL}"
        : "${BUNNY_CACHE_SIGNING_KEY:?Cache writes require a signing key}"
        : "${BUNNY_CACHE_STORAGE_ZONE:?Cache writes require a dedicated storage zone}"
        : "${BUNNY_CACHE_STORAGE_PASSWORD:?Cache writes require a storage password}"
    fi
    if [[ -z $url && -z $key ]]; then
        echo 'Bunny Hyprland cache is not configured; using normal Nix substitution/build.' >&2
        return
    fi
    [[ $url == https://* && $url != *[$'\r\n\t ']* && $url != *'?'* && $url != *'#'* && $url != *'@'* ]] || {
        echo 'Invalid BUNNY_CACHE_URL: expected an HTTPS cache URL' >&2; return 1;
    }
    [[ $key =~ ^[A-Za-z0-9._-]+:[A-Za-z0-9+/]+={0,2}$ ]] || {
        echo 'Invalid BUNNY_CACHE_PUBLIC_KEY' >&2; return 1;
    }
    # Append to the standard caches and keys; never disable signature checks.
    export NIX_CONFIG="${NIX_CONFIG:-}
extra-substituters = ${url%/}
extra-trusted-public-keys = $key
fallback = true
narinfo-cache-negative-ttl = 0"
}

prepare_hyprland_cache() {
    local snapshot=$1 evidence=$2 tooling=${3:-$1/scripts}
    # Native substitution checks exact output identities before compiling.
    # Include dev explicitly: plugin builds require generated headers too.
    nix build "path:$snapshot#hyprland" "path:$snapshot#hyprland.dev" \
        --no-update-lock-file --no-link --json --print-build-logs \
        > "$evidence/hyprland-build.json" 2> >(tee "$evidence/hyprland-build.log" >&2) || return
    if [[ ${BUNNY_CACHE_WRITE:-false} != true ]]; then
        echo 'Bunny Hyprland cache: read-only run.' >&2
        return
    fi
    # No secret enters the archived flake or evidence artifacts.
    (
        set -euo pipefail
        : "${BUNNY_CACHE_SIGNING_KEY:?Cache writes require a signing key}"
        : "${BUNNY_CACHE_STORAGE_ZONE:?Cache writes require a dedicated storage zone}"
        : "${BUNNY_CACHE_STORAGE_PASSWORD:?Cache writes require a storage password}"
        : "${BUNNY_CACHE_URL:?Cache writes require the public cache URL}"
        local staging
        staging=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/hyprland-cache.XXXXXX")
        trap 'rm -rf "$staging"' EXIT
        umask 077
        printf '%s\n' "$BUNNY_CACHE_SIGNING_KEY" > "$staging/signing-key"
        mapfile -t outputs < <(python3 - "$evidence/hyprland-build.json" <<'PY'
import json, sys
print('\n'.join(sorted({v for item in json.load(open(sys.argv[1])) for v in item['outputs'].values()})))
PY
        )
        [[ ${#outputs[@]} -gt 0 ]]
        # A fully present closure needs no upload or recompression on warm runs.
        if nix path-info --store "$BUNNY_CACHE_URL" --recursive "${outputs[@]}" > "$evidence/bunny-cache-paths.txt" 2> "$evidence/bunny-cache-check.log"; then
            echo 'Bunny Hyprland cache: complete closure already published.' >&2
            exit 0
        fi
        nix copy --to "file://$staging/cache?compression=zstd&secret-key=$staging/signing-key" "${outputs[@]}"
        python3 "$tooling/upload-bunny-cache.py" "$staging/cache"
        echo 'Bunny Hyprland cache: published signed closure.' >&2
    )
}
