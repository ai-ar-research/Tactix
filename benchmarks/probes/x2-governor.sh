#!/usr/bin/env bash
# X2 governors ported from cadical (ternaryocclim=100, ternarymaxadd=1000%),
# which we lacked entirely. THE CHAIN being tested (user decision 2026-07-30:
# do not disable X1/X2/X4 -- fix the scheduling):
#   1. govern X2 so it stops needing BCE as its bound;
#   2. mod4block should then survive BCE-off (it was 600 TO, because X2 ran to
#      32,155 resolvents once BCE stopped shrinking its pool);
#   3. BCE's occurrence cap then becomes affordable -- it was rejected ONLY
#      because it timed out mod4block, and on j3037 it is a 2.8x win.
# Arms are ungoverned-vs-governed, and the governed arm must NOT regress the
# shipped configuration (that is the kill criterion).
set -u
R=/home/hwu/SAT-solving
inst="$1"; tier="$2"; cap="$3"; arm="$4"
f=$(ls $R/benchmarks/cache/$tier/${inst}*.cnf.xz | head -1)
cnf=$TMPDIR/$(basename "$f" .cnf.xz).cnf
xz -dc "$f" > "$cnf"
case "$arm" in
  ungoverned)      export TACTIX_TERNARY_OCC_LIMIT=0 TACTIX_TERNARY_MAXADD=0 ;;
  governed)        : ;;                                   # the new defaults
  gov-nobce)       export TACTIX_SLOT_DISABLE=BCE ;;
  ungov-nobce)     export TACTIX_TERNARY_OCC_LIMIT=0 TACTIX_TERNARY_MAXADD=0; export TACTIX_SLOT_DISABLE=BCE ;;
  gov-bcecap)      export TACTIX_BCE_OCC_LIMIT=100 ;;
esac
echo "=== $inst arm=$arm ==="
/usr/bin/time -f "wall %e" timeout "$cap" $R/build/pin-x2/tactix "$cnf" 2>&1 |
  grep -E "^s |wall |^c conflicts|^c ternary|^c bce "
rm -f "$cnf"
