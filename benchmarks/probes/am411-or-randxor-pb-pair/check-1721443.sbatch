#!/usr/bin/env bash
#SBATCH --job-name=am411-dsr-recheck
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:15:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am411-or-randxor-pb-pair/check-%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am411-or-randxor-pb-pair/check-%j.err
set -euo pipefail

root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am411-or-randxor-pb-pair"
first="$probe/out/1721443"
work="$probe/recheck/$SLURM_JOB_ID"
pins="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins"
mkdir -p "$work"
printf '33873d4df74c058381c9518cb79ce39279b2156d99be781f4de6e750ab813531  %s\n' \
  "$first/input.cnf" | sha256sum -c - > "$work/input-before.log"
printf 'bd9bbe03c4e849accc03225783586a6ae5f1a55eb572b884e0bd9a69b7e4ce14  %s\n' \
  "$first/tactix/proof.out" | sha256sum -c - > "$work/proof-before.log"
sha256sum "$pins/dsr-trim" "$pins/dsr-compress" "$pins/srcheck" > "$work/pins.sha256"

set +e
timeout 600s python3 "$root/tools/bench/proof_oracle.py" check-dsr \
  "$first/input.cnf" "$first/tactix/proof.out" \
  --dsr-trim "$pins/dsr-trim" \
  --dsr-compress "$pins/dsr-compress" --srcheck "$pins/srcheck" \
  --every-lemma --workdir "$work" > "$work/check.log" 2>&1
status=$?
set -e
printf '%s\n' "$status" > "$work/check.exit"
sha256sum -c --strict "$work/pins.sha256" > "$work/pins-after.log"
printf '33873d4df74c058381c9518cb79ce39279b2156d99be781f4de6e750ab813531  %s\n' \
  "$first/input.cnf" | sha256sum -c - > "$work/input-after.log"
printf 'bd9bbe03c4e849accc03225783586a6ae5f1a55eb572b884e0bd9a69b7e4ce14  %s\n' \
  "$first/tactix/proof.out" | sha256sum -c - > "$work/proof-after.log"
cat "$work/check.log"
test "$status" -eq 0
grep -qx 's VERIFIED UNSAT' "$work/check.log"
