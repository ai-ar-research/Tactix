#!/usr/bin/env bash
set -euo pipefail
exec /home/hwu/SAT-solving/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins/dsr-trim --delete-implied-units "$@"
