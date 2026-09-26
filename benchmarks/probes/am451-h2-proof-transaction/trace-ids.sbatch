#!/usr/bin/env bash
#SBATCH --job-name=am451-h2-id-trace
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=2
#SBATCH --mem=8G
#SBATCH --time=00:10:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/am451-h2-proof-transaction/out/%j.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/am451-h2-proof-transaction/out/%j.err
set -euo pipefail
root=/home/hwu/SAT-solving
probe="$root/benchmarks/probes/am451-h2-proof-transaction"
old="$root/benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/triage/dsr-watch"
out="$probe/out/$SLURM_JOB_ID"
mkdir -p "$out"
test "$(sha256sum "$probe/driver" | cut -d ' ' -f1)" = 58212b78f43ed4ac7dcb5f44e7d0225fd33fa1064419685e720db4d9dce1b823
gdb -batch -x "$probe/gdb-ids.commands" --args "$probe/driver" \
  "$old/case.cnf" "$old/prog-replay-nox5.txt" "$out/proof.dsr" \
  3 43 api 369229852 > "$out/gdb.log" 2>&1
test "$(sha256sum "$out/proof.dsr" | cut -d ' ' -f1)" = 7076a5a85e54986793fc93c5446ed61ee4b0dcca7f6b15831976e2a7cf7a14f1
rg -q 'ADD_UNIT' "$out/gdb.log"
