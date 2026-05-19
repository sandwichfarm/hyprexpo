#!/usr/bin/env bash
set -euo pipefail

./scripts/build-site.sh
python3 -m http.server 4173 --directory dist
