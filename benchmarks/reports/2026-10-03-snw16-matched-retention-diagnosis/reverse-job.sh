#!/bin/bash
set -euo pipefail
cd /home/hwu/SAT-solving
python3 /home/hwu/SAT-solving/benchmarks/runs/20261003-snw16-retention-reverse-diagnosis/verify.py
set +e
python3 /home/hwu/SAT-solving/benchmarks/runs/20261003-snw16-retention-reverse-diagnosis/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20261003-snw16-retention-reverse-diagnosis 0
first=$?
python3 /home/hwu/SAT-solving/benchmarks/runs/20261003-snw16-retention-reverse-diagnosis/pins/runtime_task.py /home/hwu/SAT-solving/benchmarks/runs/20261003-snw16-retention-reverse-diagnosis 1
second=$?
set -e
python3 /home/hwu/SAT-solving/benchmarks/runs/20261003-snw16-retention-reverse-diagnosis/verify.py
printf '%s %s\n' "$first" "$second" > /home/hwu/SAT-solving/benchmarks/runs/20261003-snw16-retention-reverse-diagnosis/exit-codes.txt
# Diagnostic completion preserves timeouts/failures; it is not gate acceptance.
