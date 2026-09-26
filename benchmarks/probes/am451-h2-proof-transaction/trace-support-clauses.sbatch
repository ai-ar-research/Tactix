#!/usr/bin/env bash
#SBATCH --job-name=am451-h2-clauses
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
test "$(sha256sum "$probe/driver-trace" | cut -d ' ' -f1)" = 8a20c650ee6ade2e4b285bab00ca246b203bb6255f8ab02c0a22875984436bb4
TACTIX_PROOF_SUPPORT_TRACE=1 "$probe/driver-trace" "$old/case.cnf" \
  "$old/prog-replay-nox5.txt" "$out/proof.dsr" 3 43 api 369229852 \
  > "$out/solver.log" 2>&1
test "$(sha256sum "$out/proof.dsr" | cut -d ' ' -f1)" = 7076a5a85e54986793fc93c5446ed61ee4b0dcca7f6b15831976e2a7cf7a14f1
rg -q '^UNIT_SUPPORT_FOR 17$' "$out/solver.log"
