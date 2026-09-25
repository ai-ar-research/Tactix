#!/usr/bin/env bash
#SBATCH --job-name=am412-reorder-sat
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:05:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am412-reorder-after-sat-current/slurm-%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am412-reorder-after-sat-current/slurm-%j.err
set -euo pipefail
ulimit -c 0

root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am412-reorder-after-sat-current"
case_dir="$root/benchmarks/runs/20260925-am382-proof-import-claude-fuzz/handoffs/h2-reorder-after-sat-invariant"
program="${AM412_PROG:-$case_dir/prog.txt}"
driver="${AM412_DRIVER:-$root/build/pin-am412-reorder-debug/tests/xdrv-current}"
out="$probe/out/$SLURM_JOB_ID"
mkdir -p "$out"
sha256sum "$driver" "$case_dir/input.cnf" "$program" > "$out/inputs.sha256"

set +e
timeout 60s "$driver" "$case_dir/input.cnf" "$program" \
  "$out/none.proof" 2 -1 api 21000183 \
  > "$out/driver.stdout" 2> "$out/driver.stderr"
status=$?
set -e
printf '%s\n' "$status" > "$out/driver.exit"
sha256sum -c --strict "$out/inputs.sha256" > "$out/inputs-after.log"
printf 'driver exit %s\n' "$status"
