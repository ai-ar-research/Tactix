#!/usr/bin/env bash
# Focused production comparison for the two cheap Tier-A hard cases used by
# the cross-solver diagnosis.  Every arm writes DRAT and every proof is checked.
set -u

ROOT=/home/hwu/SAT-solving
NEW=${TIER_A_NEW:?set TIER_A_NEW}
OLD=${TIER_A_OLD:?set TIER_A_OLD}
KISSAT=${TIER_A_KISSAT:-$ROOT/tools/solvers/bin/kissat}
CAP=${TIER_A_CAP:-40}
CASES=${TIER_A_CASES:-"tr-n260-s002 tr-n300-s055"}
DRAT=$ROOT/tools/checkers/bin/drat-trim
SCRATCH=${TMPDIR:-/tmp}/tier-a-hard-pair.${SLURM_JOB_ID:-$$}
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

run_one() {
  local arm=$1 bin=$2 name=$3
  local proof=$SCRATCH/$arm-$name.drat
  local log=$SCRATCH/$arm-$name.log timing=$SCRATCH/$arm-$name.time rc
  echo "=== arm=$arm instance=$name cap=$CAP node=$(hostname -s) ==="
  echo "bin=$bin sha=$(sha256sum "$bin" | cut -c1-16)"
  if [ "$arm" = kissat ]; then
    /usr/bin/time -f 'wall=%e maxrss_kb=%M rc=%x' -o "$timing" \
      timeout --preserve-status --signal=TERM --kill-after=5 "$CAP" \
      "$bin" -s "$ROOT/benchmarks/cache/tierA/$name.cnf" "$proof" \
      >"$log" 2>&1
    rc=$?
  else
    /usr/bin/time -f 'wall=%e maxrss_kb=%M rc=%x' -o "$timing" \
      timeout --preserve-status --signal=TERM --kill-after=5 "$CAP" \
      "$bin" "$ROOT/benchmarks/cache/tierA/$name.cnf" "$proof" \
      >"$log" 2>&1
    rc=$?
  fi
  cat "$timing"
  grep -E '^(s |c (conflicts|decisions|propagations|learned|timing|seconds))' \
    "$log" | tail -25
  echo "solver_rc=$rc proof_bytes=$(wc -c < "$proof")"
  if [ "$rc" -eq 20 ]; then
    "$DRAT" "$ROOT/benchmarks/cache/tierA/$name.cnf" "$proof" \
      >"$SCRATCH/$arm-$name.check" 2>&1
    grep -q 's VERIFIED' "$SCRATCH/$arm-$name.check"
    echo "proof=VERIFIED"
  else
    echo "proof=NOT_CHECKED"
  fi
  echo "=== END arm=$arm instance=$name ==="
}

for name in $CASES; do
  run_one old "$OLD" "$name"
  run_one new "$NEW" "$name"
  run_one kissat "$KISSAT" "$name"
done
