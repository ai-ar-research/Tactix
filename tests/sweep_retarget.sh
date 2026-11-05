#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/proof_checks.sh"
root=$1
solver=$2
checker_paths "$root"
cnf=$root/tests/fixtures/sweep-retarget.cnf
make_scratch scratch

# This satisfiable replay reaches a Sweep binary proof that becomes a unit
# after another root consequence. Even without a final refutation, drat-trim
# checks every emitted addition as a derivation.
set +e
"$solver" --dsr --no-binary --random-policy=2475268 "$cnf" "$scratch/proof.dsr" \
  >"$scratch/solver.stdout" 2>"$scratch/solver.stderr"
status=$?
set -e
if ((status != 10)); then
  cat "$scratch/solver.stderr"
  printf 'expected SAT exit 10, got %s\n' "$status" >&2
  exit 1
fi
"$CHECKMODEL" "$cnf" "$scratch/solver.stdout"
drat_derivation "$scratch/checker.out" "$cnf" "$scratch/proof.dsr" -f || {
  cat "$scratch/checker.out" >&2
  printf 'Sweep retarget proof prefix did not check\n' >&2
  exit 1
}
printf 'sweep_retarget: SAT path and every emitted addition checked\n'
