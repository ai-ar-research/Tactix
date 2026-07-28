#!/usr/bin/env bash
# Verify the congruence tautological-lemma fix on the instance that exposed it:
# re-solve tierC/T105.2.0 with native LRAT using the FIXED binary and check the
# certificate with both cake_lpr (verified) and lrat-check (independent).
# Pre-fix: cake_lpr "Checking failed at line 762952", lrat-check "NOT VERIFIED"
# on the tautological lemma `309268 -309268 -309297`.
set -u
R=/home/hwu/SAT-solving
CNF=$TMPDIR/T105.2.0.cnf
PROOF=$TMPDIR/T105.2.0.lrat

xz -dc "$R/benchmarks/cache/tierC/T105.2.0.cnf.xz" > "$CNF"
echo "== solve with the FIXED binary (--lrat) =="
/usr/bin/time -f "solve %e s, peak %M KB" "$R/build/fixrel/tactix" --lrat "$CNF" "$PROOF"
echo "solver exit=$?"
ls -l "$PROOF"

echo "== cake_lpr (verified checker) =="
/usr/bin/time -f "cake_lpr %e s, peak %M KB" "$R/tools/checkers/bin/cake_lpr" "$CNF" "$PROOF"
echo "cake_lpr exit=$?"

echo "== lrat-check (independent implementation) =="
"$R/tools/checkers/bin/lrat-check" "$CNF" "$PROOF" | tail -6
echo "== done =="
