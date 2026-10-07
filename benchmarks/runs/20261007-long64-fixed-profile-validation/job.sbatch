#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --time=00:05:00
#SBATCH --no-requeue
#SBATCH --job-name=long64-fixed-profile
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-long64-fixed-profile-validation/slurm-%j.log
set -euo pipefail
root=/home/hwu/SAT-solving/benchmarks/runs/20261007-long64-fixed-profile-validation/frozen-root
cd "$root"
python3 -m py_compile tools/bench/experimental_tierd.py tests/test_runtime_task.py
python3 tests/test_runtime_task.py ExperimentalCheckerProfileTest
python3 tools/bench/experimental_tierd.py --help | grep -F 'long64-fixed'
set +e
python3 tools/bench/experimental_tierd.py --campaign /tmp/never-created --dsr-checker-profile=not-a-profile --prepare >/tmp/long64-fixed-invalid-$$.out 2>&1
status=$?
set -e
test "$status" -eq 2
grep -F 'invalid choice' /tmp/long64-fixed-invalid-$$.out
rm -f /tmp/long64-fixed-invalid-$$.out
