#!/usr/bin/env bash
# V5 Sweep · S-b and S-e: every proof Sweep produces, replayed through the REAL
# checkers (design §6: "tested by replaying the core as a resolution chain
# through drat-trim on micro cases — before any integration, the way the V6
# ladder was validated").
#
# TWO emitters, and the second one is not optional:
#
#  - test_subsolver --emit writes, per case, a DIMACS file (the environment,
#    then a unit per assumption the refutation actually used) plus the matching
#    LRAT and DRAT proofs. This checks S-b's derivations IN SUB-SOLVER SPACE.
#  - test_sweep --emit writes a complete UNSAT refutation whose middle is
#    literally S-e's output, i.e. those same derivations after the sub-solver ->
#    KERNEL translation. That translation is a separate piece of code and it is
#    where the 2026-08-05 N2 breach lived: the sub-solver's proofs verified here
#    while every lemma S-e wrote was the NEGATION of them, because the decode
#    inverted the literal parity. Checking only the first emitter is checking
#    the half that was right.
#
# Both formats matter and for different reasons (L13): drat-trim re-derives
# every lemma by RUP and so checks the LEMMAS, while an LRAT checker walks the
# hint chains and so checks the CHAINS — the thing S-b exists to produce, and
# the thing a permissive format silently accepts.
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
      grep -qi 'verif' "$T/check" \
        || { echo "FAIL: lrat-check gave no verdict on $cnf"; cat "$T/check"; exit 1; }
    fi
    cases=$((cases + 1))
  done
done
[ "$cases" -gt 0 ] || { echo "FAIL: no cases emitted"; exit 1; }
echo "sweep_proofs: $cases derivations verified by drat-trim$([ -x "$LRAT_CHECK" ] && echo ' + lrat-check')"
