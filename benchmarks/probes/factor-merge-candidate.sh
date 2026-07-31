#!/usr/bin/env bash
# THE MERGE QUESTION. P0 showed kissat runs Factor at size 5 with NO admission
# gate and takes 18.35 s on mod4block -- the instance our density gate exists
# for. Our own "Factor forced on" damage (13.16 -> 46.6 s) was measured at
# factor_size=2. So: is OUR size-5 Factor also benign on the gate's
# beneficiaries? If yes, "size 5 + no gate" ships and or_randxor (600 TO -> ~47 s)
# is worth 21.4 Tier B PAR-2.
# CONFIRMS: mod4block <= 20 s and crypto1 <= 8 s at FORCE+size5, rxor solves.
# KILLS: mod4block back at ~46 s -- then the gate stays and rxor needs a
# per-instance admission we do not have.
set -u
R=/home/hwu/SAT-solving
inst="$1"; cap="$2"
f=$(ls $R/benchmarks/cache/tierB/${inst}*.cnf.xz | head -1)
cnf=$TMPDIR/m.$SLURM_JOB_ID.cnf
xz -dc "$f" > "$cnf"
for arm in shipped force-s2 force-s5; do
  unset TACTIX_FACTOR_FORCE TACTIX_FACTOR_SIZE
  [ "$arm" = force-s2 ] && { export TACTIX_FACTOR_FORCE=1 TACTIX_FACTOR_SIZE=2; }
  [ "$arm" = force-s5 ] && { export TACTIX_FACTOR_FORCE=1 TACTIX_FACTOR_SIZE=5; }
  printf "%-24s %-10s " "$inst" "$arm"
  /usr/bin/time -f "wall %e" timeout "$cap" $R/build/pin-merge/tactix "$cnf" 2>&1 |
    grep -E "^s |wall |^c conflicts|^c factor " | tr -s ' ' | tr '\n' '|'; echo
done
rm -f "$cnf"
