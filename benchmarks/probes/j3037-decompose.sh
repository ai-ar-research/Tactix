#!/usr/bin/env bash
# j3037_9_rggt_b is 45x cadical (1162.8 vs 25.8) and 7.2x kissat (161.7) at the
# shipped all-rules-ON default, and ALONE it is +1137 s = 37.9 of the 47.2 PAR-2
# Tier C gap — ~80% of the whole deficit. Rules-off it is 138.8 s, so the ON
# composition costs it ~1000 s. Slot rate 2 permille takes it to ~300 s but pays
# that back on ContextModel/em_11/frb45/Ptn (rejected as a constant, 2026-07-29).
#
# debug-performance steps 0-3 in one window. Two questions:
#   (a) WHICH of the three ON rules costs the 1000 s, and what does it do
#       (rule-off arms here are DIAGNOSTICS for a scheduling defect, per the
#       standing baseline — not candidate defaults);
#   (b) is any of this a rule we LACK (step 3: ablate the baselines' sweep,
#       which is the one big inprocessing pass we have not built — R5/V5).
# The instance is SAT, so model-finding continuity is in play (cf. S7 Reorder:
# parity on UNSAT, 3.4x worse on SAT).
set -u
R=/home/hwu/SAT-solving
T=$R/build/release/tactix
CAD=$R/tools/solvers/bin/cadical
KIS=$R/tools/solvers/bin/kissat
f=$R/benchmarks/cache/tierC/j3037_9_rggt_b.cnf.xz
cnf=$TMPDIR/j3037.$SLURM_JOB_ID.cnf
xz -dc "$f" > "$cnf"

arm="$1"
echo "=== arm=$arm ==="
case "$arm" in
  shipped)   /usr/bin/time -f "wall %e" timeout 1250 "$T" "$cnf" ;;
  no-bce)    TACTIX_SLOT_DISABLE=BCE /usr/bin/time -f "wall %e" timeout 1250 "$T" "$cnf" ;;
  no-x2)     TACTIX_SLOT_DISABLE=HyperTernaryRes /usr/bin/time -f "wall %e" timeout 1250 "$T" "$cnf" ;;
  no-x4)     TACTIX_SLOT_DISABLE=Instantiate /usr/bin/time -f "wall %e" timeout 1250 "$T" "$cnf" ;;
  no-three)  TACTIX_SLOT_DISABLE=HyperTernaryRes,BCE,Instantiate /usr/bin/time -f "wall %e" timeout 1250 "$T" "$cnf" ;;
  cadical)   /usr/bin/time -f "wall %e" timeout 1250 "$CAD" "$cnf" ;;
  kissat)    /usr/bin/time -f "wall %e" timeout 1250 "$KIS" "$cnf" ;;
  cad-nosweep) /usr/bin/time -f "wall %e" timeout 1250 "$CAD" --sweep=false "$cnf" ;;
  kis-nosweep) /usr/bin/time -f "wall %e" timeout 1250 "$KIS" --sweep=0 "$cnf" ;;
  *) echo "unknown arm"; exit 1 ;;
esac
rm -f "$cnf"
