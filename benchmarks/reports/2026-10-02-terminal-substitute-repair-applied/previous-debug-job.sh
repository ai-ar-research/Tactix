#!/bin/bash
set -euo pipefail
cd /home/hwu/SAT-solving
python3 /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-f6f01982-preparation/verify.py
set +e
ctest --test-dir /home/hwu/SAT-solving/build/pin-shared-policy-feedback-debug-f6f01982 -j 8 --output-on-failure --output-junit /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-f6f01982-preparation/junit.xml --output-log /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-f6f01982-preparation/ctest.log
status=$?
set -e
python3 /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-f6f01982-preparation/verify.py
printf '%s\n' "$status" > /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-f6f01982-preparation/exit-code.txt
exit "$status"
