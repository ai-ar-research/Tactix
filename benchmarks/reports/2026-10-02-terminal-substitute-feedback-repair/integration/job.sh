#!/bin/bash
set -euo pipefail
ulimit -c 0
cd /home/hwu/SAT-solving
exec python3 /home/hwu/SAT-solving/benchmarks/runs/20261002-terminal-substitute-integration-validation/validate.py
