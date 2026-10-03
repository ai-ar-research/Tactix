#!/bin/bash
set -euo pipefail
cd /home/hwu/SAT-solving
python3 /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-375b621b-preparation/verify.py
set +e
ctest --test-dir /home/hwu/SAT-solving/build/pin-shared-policy-feedback-debug-375b621b -j 8 --output-on-failure --output-junit /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-375b621b-preparation/junit.xml --output-log /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-375b621b-preparation/ctest.log
status=$?
set -e
python3 /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-375b621b-preparation/verify.py
printf '%s\n' "$status" > /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-375b621b-preparation/exit-code.txt
exit "$status"
