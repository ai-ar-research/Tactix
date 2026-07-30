#!/usr/bin/env bash
# debug-performance ladder step 1/2 for the mod4block SEARCH gap. With BCE off on
# our side (and neither reference having BCE: cadical ships `block` off, kissat 4
# deleted the module), we need 10.08M conflicts and do not solve in 600 s where
# kissat needs 715,338 in 18.3 s. Conflicts are known; the DECOMPOSITION is not.
# This collects both sides' counters so the gap can be attributed to search-tree
# size vs per-conflict cost vs decision/restart cadence rather than guessed at.
# The long tactix arm also answers a separate question: is this a constant factor
# or a wall (does it EVER solve without BCE)?
set -u
R=/home/hwu/SAT-solving
f=$(ls $R/benchmarks/cache/tierB/mod4block*.cnf.xz | head -1)
cnf=$TMPDIR/m.$SLURM_JOB_ID.cnf
xz -dc "$f" > "$cnf"
case "$1" in
  tactix-nobce-long)
    TACTIX_SLOT_DISABLE=BCE /usr/bin/time -f "wall %e" timeout 3600 \
      $R/build/pin-occ/tactix "$cnf" 2>&1 | grep -vE '^v ' ;;
  tactix-bce-on)
    /usr/bin/time -f "wall %e" timeout 600 $R/build/pin-occ/tactix "$cnf" 2>&1 | grep -vE '^v ' ;;
  kissat)
    /usr/bin/time -f "wall %e" timeout 600 $R/tools/solvers/bin/kissat "$cnf" 2>&1 | grep -vE '^v ' ;;
  cadical)
    /usr/bin/time -f "wall %e" timeout 600 $R/tools/solvers/bin/cadical "$cnf" 2>&1 | grep -vE '^v ' ;;
esac
rm -f "$cnf"
