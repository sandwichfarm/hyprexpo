#!/usr/bin/env sh
# Verify that every hyprexpo commit referenced by hyprpm.toml is reachable
# from the release commit. A commit from a squash-merged PR branch is not.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ref=${1:-}
target_ref=${ref:-HEAD}
temporary_manifest=

cleanup() {
    if [ -n "$temporary_manifest" ]; then
        rm -f -- "$temporary_manifest"
    fi
}
trap cleanup 0 1 2 15

target_commit=$(
    git -C "$root" rev-parse --verify "$target_ref^{commit}" 2>/dev/null
) || {
    echo "error: '$target_ref' does not resolve to a commit"
    exit 1
}

manifest="$root/hyprpm.toml"
location="the working tree"

if [ -n "$ref" ]; then
    temporary_manifest=$(mktemp "${TMPDIR:-/tmp}/hyprexpo-commit-pins.XXXXXX")
    git -C "$root" show "$target_commit:hyprpm.toml" > "$temporary_manifest" 2>/dev/null || {
        echo "error: hyprpm.toml does not exist at '$ref'"
        exit 1
    }
    manifest="$temporary_manifest"
    location="'$ref'"
fi

pin_lines=$(
    awk -F '"' '
        /^[[:space:]]*commit_pins[[:space:]]*=/ {
            in_pins = 1
            next
        }
        in_pins && /^[[:space:]]*]/ {
            in_pins = 0
            next
        }
        in_pins && /^[[:space:]]*\[/ {
            entries++
            if (NF < 5) {
                print "INVALID:" NR
            } else {
                print $4
            }
        }
        END {
            if (entries == 0) {
                print "EMPTY"
            }
        }
    ' "$manifest"
)

checked=0
failed=0

for pin in $pin_lines; do
    case "$pin" in
        EMPTY)
            echo "error: no commit_pins entries found in hyprpm.toml at $location"
            failed=$((failed + 1))
            continue
            ;;
        INVALID:*)
            line=${pin#INVALID:}
            echo "error: malformed commit_pins entry on hyprpm.toml line $line at $location"
            failed=$((failed + 1))
            continue
            ;;
    esac

    checked=$((checked + 1))

    if [ "${#pin}" -ne 40 ]; then
        echo "error: plugin commit pin '$pin' is not a full 40-character commit hash"
        failed=$((failed + 1))
        continue
    fi

    case "$pin" in
        *[!0-9a-fA-F]*)
            echo "error: plugin commit pin '$pin' is not hexadecimal"
            failed=$((failed + 1))
            continue
            ;;
    esac

    if ! git -C "$root" cat-file -e "$pin^{commit}" 2>/dev/null; then
        echo "error: plugin commit pin '$pin' is unavailable from $target_ref"
        failed=$((failed + 1))
        continue
    fi

    if ! git -C "$root" merge-base --is-ancestor "$pin" "$target_commit"; then
        echo "error: plugin commit pin '$pin' is not on the history of $target_ref"
        failed=$((failed + 1))
    fi
done

if [ "$failed" -ne 0 ]; then
    echo
    echo "hyprpm plugin pins must be fetchable from the released repository history."
    echo "After a squash merge, replace the PR-branch hash with the commit that"
    echo "actually landed on master, commit hyprpm.toml, and run this check again."
    exit 1
fi

echo "verified $checked hyprpm plugin commit pins against $target_ref ($target_commit)"
