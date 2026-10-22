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
# All three formats are required: drat-trim re-derives every lemma by RUP,
# lrat-check walks the hint chains, and strict dsr-trim plus Trestle exercises
# native DSR's implied-unit state across the general Sweep transaction.
#
# Exit 77 (ctest SKIP) without a checker.
set -euo pipefail
ROOT="$1"
SOLVER="$2"
shift 2
DRAT_TRIM="${DRAT_TRIM:-$ROOT/tools/checkers/bin/drat-trim}"
LRAT_CHECK="${LRAT_CHECK:-$ROOT/tools/checkers/bin/lrat-check}"
DSR_TRIM="${DSR_TRIM:-$ROOT/tools/checkers/bin/dsr-trim}"
SR_CHECK="${SR_CHECK:-$ROOT/tools/checkers/bin/srcheck}"
DSR_COMPRESS="${DSR_COMPRESS:-$ROOT/tools/checkers/bin/dsr-compress}"
# tools/bench/proof_oracle.py owns the reference DSR checking pipeline and
# the forward audit that checks every line of these small proofs.
check_dsr() {
  python3 "$ROOT/tools/bench/proof_oracle.py" check-dsr "$1" "$2" \
    --dsr-trim "$DSR_TRIM" --dsr-compress "$DSR_COMPRESS" \
    --srcheck "$SR_CHECK" --every-lemma > "$T/check" 2>&1
}
[ -x "$DRAT_TRIM" ] || { echo "drat-trim not built — skipping"; exit 77; }

T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT

cases=0
dsr_cases=0
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
    if [ -f "$base.dsr" ] && [ -x "$DSR_TRIM" ] && [ -x "$SR_CHECK" ]; then
      check_dsr "$cnf" "$base.dsr" \
        || { echo "FAIL: reference DSR pipeline rejected $cnf"; cat "$T/check"; exit 1; }
      dsr_cases=$((dsr_cases + 1))
    fi
    cases=$((cases + 1))
  done
done

# The compact emitters above exercise the proof vocabulary. These two
# trajectory-sensitive cases pin the transaction around that vocabulary:
# 031 requires preserving an implied unit before its transient Sweep reason is
# deleted, while 032 requires deleting that reason so it cannot add a later
# RAT resolvent obligation.
if [ -x "$DSR_TRIM" ] && [ -x "$SR_CHECK" ]; then
  while read -r name seed cnf; do
    proof="$T/$name.dsr"
    lsr="$T/$name.lsr"
    set +e
    "$SOLVER" --dsr --random-policy="$seed" \
      "$ROOT/$cnf" "$proof" >"$T/$name.solver" 2>"$T/$name.stderr"
    solver_rc=$?
    set -e
    [ "$solver_rc" -eq 20 ] || {
      echo "FAIL: $name solver exit $solver_rc (expected UNSAT/20)"
      cat "$T/$name.solver" "$T/$name.stderr"
      exit 1
    }
    check_dsr "$ROOT/$cnf" "$proof" || {
      echo "FAIL: reference DSR pipeline rejected $name"
      cat "$T/check"
      exit 1
    }
    cases=$((cases + 1))
    dsr_cases=$((dsr_cases + 1))
  done <<'EOF'
sweep-dsr-unit-reason 296485266 tools/fuzz/corpus/regressions/031-sweep-dsr-implied-unit-reason/case.cnf
sweep-dsr-rat-population 296305338 tools/fuzz/corpus/regressions/032-sweep-dsr-retained-prefix-rat-population/min.cnf
sweep-dsr-live-reason-unit 296326708 tools/fuzz/corpus/regressions/033-sweep-dsr-live-reason-shadow-unit/case.cnf
EOF
fi
[ "$cases" -gt 0 ] || { echo "FAIL: no cases emitted"; exit 1; }
echo "sweep_proofs: $cases derivations verified by drat-trim$([ -x "$LRAT_CHECK" ] && echo ' + lrat-check')$([ "$dsr_cases" -gt 0 ] && echo " + strict dsr-trim/Trestle ($dsr_cases DSR cases)")"
