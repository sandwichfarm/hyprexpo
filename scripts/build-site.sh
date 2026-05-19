#!/usr/bin/env bash
set -euo pipefail

if [ ! -d "site/node_modules" ]; then
    if [ -f "site/package-lock.json" ]; then
        npm ci --prefix site
    else
        npm install --prefix site
    fi
fi

npm run build --prefix site
./scripts/build-docs.sh
./scripts/verify-site-dist.sh
