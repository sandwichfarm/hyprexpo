#!/usr/bin/env bash
# Validate a proposed persistent revision before hyprpm writes its registration.
set -euo pipefail

if (( $# < 1 || $# > 2 )); then
    echo "usage: $0 <repository-url> [revision]" >&2
    exit 2
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
python3 "$script_dir/check-hyprpm-state.py" --url="$1" --revision="${2:-}"
exec hyprpm add "$@"
