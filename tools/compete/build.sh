#!/usr/bin/env bash
# SAT Competition 2026 build entry point.
set -euo pipefail
cd "$(dirname "$0")/../.."
cmake --preset release
cmake --build --preset release --parallel
