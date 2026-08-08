#!/usr/bin/env bash
# SAT Competition 2026 build entry point.
set -euo pipefail
cmake --preset release
cmake --build --preset release --parallel
