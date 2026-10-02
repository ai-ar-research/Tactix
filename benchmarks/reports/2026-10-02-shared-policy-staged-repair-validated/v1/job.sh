#!/bin/bash
set -euo pipefail
cd /home/hwu/SAT-solving
python3 benchmarks/runs/20261002-shared-policy-staged-repair-7e2c5eb1/verify.py
set +e
python3 benchmarks/runs/20261002-shared-policy-staged-repair-7e2c5eb1/run.py
status=$?
set -e
python3 benchmarks/runs/20261002-shared-policy-staged-repair-7e2c5eb1/verify.py
printf '%s\n' "$status" > benchmarks/runs/20261002-shared-policy-staged-repair-7e2c5eb1/exit-code.txt
exit "$status"
