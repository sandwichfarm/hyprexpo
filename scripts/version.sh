#!/usr/bin/env sh
# Single source of truth for the hyprexpo version.
#
#   scripts/version.sh         -> version string to bake into the binary
#   scripts/version.sh --base  -> raw VERSION file contents (the release version)
#
# The VERSION file holds the canonical release version (e.g. v0.55.2+2). It is
# kept in lockstep with the git tag by `make tag`, so the version reported by
# the plugin, the VERSION file, and the tag can never drift.
#
# For builds that are NOT sitting exactly on the release tag (i.e. development
# builds between releases) a "-dev+<shorthash>" suffix is appended so crash
# reports can tell a real release apart from an in-between build.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
version_file="$root/VERSION"

base="v0.0.0-dev"
if [ -r "$version_file" ]; then
    file_contents=$(tr -d ' \t\r\n' < "$version_file")
    [ -n "$file_contents" ] && base="$file_contents"
fi

if [ "${1:-}" = "--base" ]; then
    printf '%s\n' "$base"
    exit 0
fi

# Baked version: exactly the tag when built on it, otherwise a dev marker.
if git -C "$root" rev-parse --git-dir >/dev/null 2>&1; then
    exact=$(git -C "$root" describe --tags --exact-match 2>/dev/null || true)
    if [ "$exact" = "$base" ]; then
        printf '%s\n' "$base"
    else
        short=$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)
        printf '%s-dev+%s\n' "$base" "$short"
    fi
else
    printf '%s\n' "$base"
fi
