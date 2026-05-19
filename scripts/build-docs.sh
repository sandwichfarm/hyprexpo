#!/usr/bin/env bash
set -euo pipefail

if [ ! -x "docs/node_modules/.bin/vitepress" ]; then
    if [ -f "docs/package-lock.json" ]; then
        npm ci --prefix docs
    else
        npm install --prefix docs
    fi
fi

./docs/node_modules/.bin/vitepress build docs
node scripts/prepare-vitepress-directory-urls.mjs
