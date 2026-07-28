#!/usr/bin/env bash
# Wrong-SAT from the random-policy fuzz legs (case 2204, shrunk to 6 clauses).
# Discriminator: does it reproduce on the PRE-SESSION binary (build/release,
# which has neither X1 BCE nor X2 HyperTernaryRes nor the LRAT fix)?
#   reproduces on old  => pre-existing random-policy defect, not Track R
#   only on new        => my new rules (BCE deletes with witnesses => prime suspect)
set -u
R=/home/hwu/SAT-solving
C=$R/tools/fuzz/failures/002204-model-seed20262913/min.cnf

for bin in build/release/tactix build/fixrel/tactix build/fixdbg/tactix; do
  echo "=================== $bin ==================="
  "$R/$bin" --random-policy "$C" | grep -E "^s |^v " | head -3
  echo "--- rule stats ---"
  "$R/$bin" --random-policy "$C" | grep -E "bve|congruence|lucky|walk" | head -6
done

echo "=================== registry ==================="
# which rules each binary knows about (the new ones only exist in the fix builds)
for bin in build/release/tactix build/fixrel/tactix; do
  echo "$bin:"; "$R/$bin" --version
done
echo "== done =="
