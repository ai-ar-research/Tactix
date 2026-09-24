#!/usr/bin/env bash
# A bounded checker-route diagnostic for the same frozen ncc proof, not a gate.
# The proof and elaborations remain in tmp/ until Codex inspects the result.
#SBATCH --job-name=am361-ncc-lsr-cake
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=32768M
#SBATCH --time=18:00:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260924-am361-ncc-lsr-cake-probe/logs/%j.log
set -euo pipefail
ulimit -c 0
export LC_ALL=C

root=/home/hwu/SAT-solving
run=$root/benchmarks/runs/20260924-am361-ncc-lsr-cake-probe
pin=$root/benchmarks/runs/20260924-am343-routed-diagnostic/pins
compressed=$root/benchmarks/cache/tierD24-26/9d7caee59e9d362d132f1cd785de8a0b-ncc_none_21015_5_3_3_0_0_11.cnf.xz
expected_proof=4620a6e8d0647bc381da13286235c8ae5b0e2d753e3fcb12488611c2c2c9f84d

check_hash() {
  local got
  got=$(sha256sum "$2")
  got=${got%% *}
  test "$got" = "$1" || {
    echo "PIN MISMATCH: $2 expected=$1 actual=$got"
    exit 1
  }
}
check_hash cbe999105947efd3782bf4abc7ec469136ec0b0551322df1039bd5e250e596c2 "$pin/tactix"
check_hash 3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee "$pin/dsr-trim"
check_hash a44102ce11162a4687450a4c3a640d27a49703072dd29802bd259676eb668d55 "$pin/dsr-compress"
check_hash ab9dc6c43ad0f55bbaf336c5d898919ddb03d0bdb75e88dff48237fb06ba1f57 "$pin/cake_lpr"
check_hash 19688ec103039ff836ee6329ac48e569ef1a722659168f85df2d070350c0e728 "$compressed"

scratch=$(mktemp -d "$run/tmp/job-${SLURM_JOB_ID}.XXXXXXXX")
echo "scratch=$scratch"
echo "job=${SLURM_JOB_ID} node=$(hostname)"
xz -dc "$compressed" > "$scratch/original.cnf"
check_hash a46993674fbda5a533d22389e9fb93566569f750b9c7ec100b2f19c8edde66ed "$scratch/original.cnf"

solver_start=$(date +%s)
set +e
timeout --signal=TERM --kill-after=30s 1200 "$pin/tactix" \
  "$scratch/original.cnf" "$scratch/proof.dsr" > "$scratch/solver.out" 2> "$scratch/solver.err"
solver_rc=$?
set -e
echo "solver_exit=$solver_rc solver_seconds=$(($(date +%s)-solver_start))"
test "$solver_rc" -eq 20 || { echo 'SOLVER DID NOT REFUTE'; exit 1; }
check_hash "$expected_proof" "$scratch/proof.dsr"
wc -c "$scratch/proof.dsr"

check_start=$(date +%s)
remaining() {
  local left=$((57600 - $(date +%s) + check_start))
  if ((left > 0)); then echo "$left"; else echo 0; fi
}
left=$(remaining)
test "$left" -gt 0 || { echo 'CHECKER WINDOW EXHAUSTED BEFORE DSR'; exit 1; }
dsr_start=$(date +%s)
timeout --signal=TERM --kill-after=30s "$left" "$pin/dsr-trim" \
  "$scratch/original.cnf" "$scratch/proof.dsr" "$scratch/proof.binary.lsr" \
  | tee "$scratch/dsr-trim.log"
echo "dsr_trim_seconds=$(($(date +%s)-dsr_start))"
grep -Fxq 's VERIFIED UNSAT' "$scratch/dsr-trim.log" || {
  echo 'DSR-TRIM DID NOT VERIFY'
  exit 1
}
test -s "$scratch/proof.binary.lsr"

left=$(remaining)
test "$left" -gt 0 || { echo 'CHECKER WINDOW EXHAUSTED BEFORE DECODE'; exit 1; }
timeout --signal=TERM --kill-after=30s "$left" "$pin/dsr-compress" -d \
  "$scratch/proof.binary.lsr" "$scratch/proof.lsr"
test -s "$scratch/proof.lsr"

left=$(remaining)
test "$left" -gt 0 || { echo 'CHECKER WINDOW EXHAUSTED BEFORE CAKE'; exit 1; }
cake_start=$(date +%s)
CML_HEAP_SIZE=24576 timeout --signal=TERM --kill-after=30s "$left" \
  "$pin/cake_lpr" "$scratch/original.cnf" "$scratch/proof.lsr" \
  | tee "$scratch/cake.log"
echo "cake_seconds=$(($(date +%s)-cake_start)) checker_seconds=$(($(date +%s)-check_start))"
grep -Fxq 's VERIFIED UNSAT' "$scratch/cake.log" || {
  echo 'CAKE LPR DID NOT VERIFY'
  exit 1
}
sha256sum "$scratch/proof.dsr" "$scratch/proof.binary.lsr" "$scratch/proof.lsr"
wc -c "$scratch/proof.dsr" "$scratch/proof.binary.lsr" "$scratch/proof.lsr"
echo 'RESULT: VERIFIED ORIGINAL-CNF DSR->LSR->CakeLPR (diagnostic only)'
