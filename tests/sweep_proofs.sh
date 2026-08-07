#!/usr/bin/env bash
# V5 Sweep, rules S-b and S-e: every proof Sweep produces, replayed through the
# real checkers.
#
# Both emitters are required, because they cover different code:
#  - test_subsolver --emit writes, per case, a DIMACS file (the environment plus
#    a unit per assumption the refutation used) and the matching LRAT and DRAT
#    proofs. That covers S-b's derivations in sub-solver space.
#  - test_sweep --emit writes a complete UNSAT refutation whose middle is S-e's
#    output: the same derivations after the sub-solver -> kernel translation,
#    which is separate code and can invert literal parity while sub-solver-space
#    proofs still verify.
#
# Both formats are required too: drat-trim re-derives every lemma by RUP and so
# checks the lemmas, while an LRAT checker walks the hint chains and so checks
# the chains, which is what S-b exists to produce.
#
# Exit 77 (ctest SKIP) without a checker.
set -euo pipefail
ROOT="$1"
shift
DRAT_TRIM="$ROOT/tools/checkers/bin/drat-trim"
LRAT_CHECK="$ROOT/tools/checkers/bin/lrat-check"
[ -x "$DRAT_TRIM" ] || { echo "drat-trim not built — skipping"; exit 77; }

T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT

cases=0
for emitter in "$@"; do
  D="$T/$(basename "$emitter")"
  mkdir -p "$D"
  "$emitter" --emit "$D"
  for cnf in "$D"/*.cnf; do
    base="${cnf%.cnf}"
    # judge by output, not exit code: drat-trim exits nonzero on "trivial UNSAT"
    # even when it prints s VERIFIED
    "$DRAT_TRIM" "$cnf" "$base.drat" > "$T/check" 2>&1 || true
    grep -q 's VERIFIED' "$T/check" \
      || { echo "FAIL: DRAT of $cnf rejected"; cat "$T/check"; exit 1; }
    if [ -x "$LRAT_CHECK" ]; then
      "$LRAT_CHECK" "$cnf" "$base.lrat" > "$T/check" 2>&1 \
        || { echo "FAIL: LRAT of $cnf rejected"; cat "$T/check"; exit 1; }
      # Test for rejection first: lrat-check's rejection verdict contains its
      # acceptance verdict as a substring, so a bare `verif` match would accept
      # both and turn every rejection into a pass.
      if grep -qi 'not verified' "$T/check"; then
        echo "FAIL: lrat-check REJECTED the LRAT of $cnf"; cat "$T/check"; exit 1
      fi
      grep -qi 'verified' "$T/check" \
        || { echo "FAIL: lrat-check gave no verdict on $cnf"; cat "$T/check"; exit 1; }
    fi
    cases=$((cases + 1))
  done
done
[ "$cases" -gt 0 ] || { echo "FAIL: no cases emitted"; exit 1; }
echo "sweep_proofs: $cases derivations verified by drat-trim$([ -x "$LRAT_CHECK" ] && echo ' + lrat-check')"
