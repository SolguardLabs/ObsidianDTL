#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

node scripts/check-format.mjs
node scripts/build.mjs
node --test --test-concurrency=1 tests/node/*.test.js
