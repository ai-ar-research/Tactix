#!/bin/bash
set -euo pipefail
cd /home/hwu/SAT-solving
python3 /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-0531c2f1-preparation/verify.py
set +e
ctest --test-dir /home/hwu/SAT-solving/build/pin-shared-policy-feedback-debug-0531c2f1 -j 8 --output-on-failure --output-junit /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-0531c2f1-preparation/junit.xml --output-log /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-0531c2f1-preparation/ctest.log
status=$?
set -e
python3 /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-0531c2f1-preparation/verify.py
printf '%s\n' "$status" > /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-0531c2f1-preparation/exit-code.txt
exit "$status"
