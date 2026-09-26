#!/usr/bin/env bash
set -euo pipefail
root=$1
solver=$2
checker=${CAKE_LPR:-$root/tools/checkers/bin/cake_lpr}
cnf=$root/tests/fixtures/sweep-retarget-lrat.cnf
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT

# This satisfiable replay reaches a Sweep binary proof that becomes a unit
# after another root consequence. Even without a final refutation, CakeLPR
# checks every emitted LRAT addition before reporting the absent empty clause.
set +e
"$solver" --lrat --random-policy=2475268 "$cnf" "$scratch/proof.lrat" \
  >"$scratch/solver.stdout" 2>"$scratch/solver.stderr"
status=$?
set -e
if ((status != 10)); then
  cat "$scratch/solver.stderr"
  printf 'expected SAT exit 10, got %s\n' "$status" >&2
  exit 1
fi
"$root/tools/fuzz/checkmodel" "$cnf" "$scratch/solver.stdout"
"$checker" "$cnf" "$scratch/proof.lrat" >"$scratch/checker.out" 2>&1
if ! grep -Fxq 'c empty clause not derived at end of proof' "$scratch/checker.out"; then
  cat "$scratch/checker.out" >&2
  printf 'Sweep retarget LRAT prefix did not check\n' >&2
  exit 1
fi
printf 'sweep_retarget_lrat: SAT path and every emitted LRAT addition checked\n'
