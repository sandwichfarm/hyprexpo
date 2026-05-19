#!/usr/bin/env bash
set -euo pipefail

if [ ! -x "docs/node_modules/.bin/vitepress" ]; then
    if [ -f "docs/package-lock.json" ]; then
        npm ci --prefix docs
    else
        npm install --prefix docs
    fi
fi

exec ./docs/node_modules/.bin/vitepress dev docs "$@"
