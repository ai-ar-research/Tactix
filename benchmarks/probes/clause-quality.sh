#!/usr/bin/env bash
# Clause-quality decomposition vs kissat (phase-3 item 5). The conflict-count
# gap (FmlaEquiv 1.53x) is a SYMPTOM; this asks what the clauses we learn
# actually look like next to kissat's on the same instances:
#   - mean learned size and mean glue
#   - how much minimize+shrink removes
#   - conflicts, restarts, and props/conflict
# Instances: the three the TB5 reframe named as the residual carriers.
set -u
R=/home/hwu/SAT-solving
T=$R/build/probe/tactix
K=$R/tools/solvers/bin/kissat

for inst in tierB/FmlaEquivChain_4_6_6.sanitized.cnf.xz \
            tierB/9vliw_bp_mc.shuffled.cnf.xz \
            tierB/x9-08014.sat.sanitized.cnf.xz \
            tierB/hwmcc17miters-xits-iso-oski15a08b00s.sanitized.cnf.xz; do
  name=$(basename "$inst" .cnf.xz)
  cnf=$TMPDIR/$name.cnf
  xz -dc "$R/benchmarks/cache/$inst" > "$cnf"
  echo "################ $name ################"
  echo "--- tactix ---"
  "$T" "$cnf" | grep -E "^c (conflicts|learned |learnedsize|reduction|minimized|shrunken|restarts|propagations|mode)"
  echo "--- kissat ---"
  "$K" -s "$cnf" 2>/dev/null | grep -E "^c (conflicts|learned|clause|size|glue|minimized|shrunken|restarts|propagations)" | head -20
  rm -f "$cnf"
done
echo "== done =="
