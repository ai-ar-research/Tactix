#!/usr/bin/env bash
#SBATCH --job-name=am395-postfinal-current
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:15:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260925-am395-current-postfinal/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/runs/20260925-am395-current-postfinal/out/%j.err
set -euo pipefail

run=/home/hwu/SAT-solving/benchmarks/runs/20260925-am395-current-postfinal
test "$(sha256sum "$run/driver" | cut -d ' ' -f 1)" = \
  "$(sed -n '2s/ .*//p' "$run/sha256.txt")"
for entry in none cand status applicable instances; do
  set +e
  "$run/driver" "$entry" > "$run/out/$entry.txt" 2>&1
  code=$?
  set -e
  printf '%s exit=%s ' "$entry" "$code"
  tr '\n' ' ' < "$run/out/$entry.txt"
  printf '\n'
done
