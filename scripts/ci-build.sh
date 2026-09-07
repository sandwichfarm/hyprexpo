#!/usr/bin/env bash
# Build an immutable snapshot with an exact upstream and its transitive lock.
set -euo pipefail

rev=${1:?usage: ci-build.sh UPSTREAM_COMMIT EVIDENCE_DIRECTORY}
evidence=${2:?usage: ci-build.sh UPSTREAM_COMMIT EVIDENCE_DIRECTORY}
[[ $rev =~ ^[0-9a-f]{40}$ ]] || { echo 'Expected a full upstream commit' >&2; exit 2; }
root=$(git rev-parse --show-toplevel)
mkdir -p "$evidence"
evidence=$(realpath "$evidence")
snapshot=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/hyprexpo-ci.XXXXXX")
trap 'rm -rf "$snapshot"' EXIT
git -C "$root" archive HEAD | tar -x -C "$snapshot"
cd "$snapshot"

{
    printf 'plugin_commit=%s\n' "$(git -C "$root" rev-parse HEAD)"
    printf 'plugin_tree=%s\n' "$(git -C "$root" rev-parse HEAD^{tree})"
    printf 'hyprland_commit=%s\n' "$rev"
    printf 'run_url=%s/%s/actions/runs/%s\n' "${GITHUB_SERVER_URL:-local}" "${GITHUB_REPOSITORY:-local}" "${GITHUB_RUN_ID:-local}"
    nix --version
    uname -a
} > "$evidence/provenance.txt"

flake_args=(--override-input hyprland "github:hyprwm/Hyprland/$rev" --no-write-lock-file)
if [[ -n ${CI_TARGET_BRANCH:-} ]]; then
    # Validate the branch's own pin before overriding it for a matrix cell.
    nix flake metadata --json --no-update-lock-file "path:$snapshot" > "$evidence/branch-metadata.json"
    python3 scripts/ci-targets.py verify-branch-lock "$evidence/branch-metadata.json" "$CI_TARGET_BRANCH"
fi
nix flake metadata --json "${flake_args[@]}" "path:$snapshot" > "$evidence/metadata.json"
python3 scripts/ci-targets.py verify-lock "$evidence/metadata.json" "$rev"
# Freeze the resolved complete dependency environment before building.
python3 - "$evidence/metadata.json" <<'PY'
import json, sys
from pathlib import Path
Path("flake.lock").write_text(json.dumps(json.load(open(sys.argv[1]))["locks"], indent=2) + "\n")
PY
cp flake.lock "$evidence/flake.lock"
sha256sum flake.lock >> "$evidence/provenance.txt"

# Nix store paths bind source, generated headers and dependencies together.
# Establish an output first: --rebuild refuses never-built derivations. Then
# force compilation and compare it with the first output, including cache hits.
nix build "path:$snapshot#hyprexpo" "${flake_args[@]}" --no-update-lock-file \
    --no-link --json --print-build-logs \
    > "$evidence/build.json" 2> >(tee "$evidence/build.log" >&2)
nix build "path:$snapshot#hyprexpo" "${flake_args[@]}" --no-update-lock-file \
    --rebuild --no-link --json --print-build-logs \
    > "$evidence/rebuild.json" 2> >(tee "$evidence/rebuild.log" >&2)
output=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))[0]["outputs"]["out"])' "$evidence/build.json")
drv=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))[0]["drvPath"])' "$evidence/build.json")
nix derivation show "$drv" > "$evidence/derivation.json"
nix path-info --recursive --json "$output" > "$evidence/closure.json"
mapfile -t plugins < <(find "$output" -type f \( -name hyprexpo.so -o -name libhyprexpo.so \))
[[ ${#plugins[@]} -eq 1 ]] || { echo 'Expected exactly one plugin artifact' >&2; exit 1; }
cp "${plugins[0]}" "$evidence/hyprexpo.so"
sha256sum "$evidence/hyprexpo.so" >> "$evidence/provenance.txt"

# Export the exact generated headers used by this package set for audit.
nix build "path:$snapshot#hyprland.dev" "${flake_args[@]}" --no-update-lock-file \
    --no-link --json > "$evidence/headers-build.json"
headers=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))[0]["outputs"]["dev"])' "$evidence/headers-build.json")
find "$headers" -type f \( -name version.h -o -path '*/include/hyprland/protocols/*' \) -print0 \
    | sort -z | xargs -0 -r sha256sum > "$evidence/generated-headers.sha256"
[[ -s "$evidence/generated-headers.sha256" ]] || { echo 'No generated header provenance found' >&2; exit 1; }
