#!/usr/bin/env bash
#SBATCH --job-name=am464-bad-proof
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --time=00:05:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am464-split-production/out/%j.negative.out
set -euo pipefail
root=/home/hwu/SAT-solving
work="$root/benchmarks/probes/scratch/am464-split-production"
pin="$root/benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/pins/checkers"
good="$work/out/1728109_0/fmt4.dsr"
cnf="$work/out/1728109_0/checker.cnf"
out="$work/out/$SLURM_JOB_ID-negative"
mkdir -p "$out"
printf '0\n' > "$out/empty-only.dsr"
head -n "$(( $(wc -l < "$good") / 2 ))" "$good" > "$out/truncated.dsr"
printf '0\n' >> "$out/truncated.dsr"
for name in empty-only truncated; do
  for direction in backward forward; do
    opts=()
    if [[ "$direction" == forward ]]; then opts=(-f); fi
    rc=0
    "$pin/dsr-trim" "${opts[@]}" "$cnf" "$out/$name.dsr" "$out/$name-$direction.lsr" > "$out/$name-$direction.log" 2>&1 || rc=$?
    if [[ "$rc" == 0 ]] || rg -q '^s VERIFIED UNSAT$' "$out/$name-$direction.log"; then
      echo "BAD PROOF ACCEPTED: $name $direction" >&2
      exit 1
    fi
    printf '%s\t%s\trejected\t%s\n' "$name" "$direction" "$rc"
  done
done
