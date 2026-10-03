#!/bin/bash
#SBATCH --job-name=ramsey-full-debug
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=32768M
#SBATCH --time=08:00:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261003-ramsey-policy-paths/debug-slurm-%j.log
set -euo pipefail
cd /home/hwu/SAT-solving
python3 benchmarks/runs/20261003-ramsey-policy-paths/verify-full-debug.py
set +e
ctest --test-dir build/pin-ramsey-full-fb174175-debug-01 -j 8 --output-on-failure --output-junit /home/hwu/SAT-solving/benchmarks/runs/20261003-ramsey-policy-paths/debug-junit.xml --output-log /home/hwu/SAT-solving/benchmarks/runs/20261003-ramsey-policy-paths/debug-ctest.log
status=$?
set -e
python3 benchmarks/runs/20261003-ramsey-policy-paths/verify-full-debug.py
printf '%s\n' "$status" > benchmarks/runs/20261003-ramsey-policy-paths/debug-exit-code.txt
exit "$status"
