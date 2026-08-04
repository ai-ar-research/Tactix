#!/usr/bin/env bash
# `unsat-set-b-fclqcolor-10-07-09` — the LAST Tier B timeout once broad Factor
# admission ships. Worth ~19.5 PAR-2 and the 28th solve.
#
# WHY THIS PROBE, AND WHY THE OLD ONE IS STALE.  The 2026-07-30 diagnosis filed
# fclq as cause B (rule-SCOPE: "same missing BVA" as or_randxor) on the strength
# of `kissat --factorsize=2` — kissat restricted to the BINARY-clause Factor we
# implemented then — costing it 12.6x there.  Our variant has since changed
# twice: `factor_size = 5` (kissat's own default) shipped 07-31, and broad
# admission (`TACTIX_FACTOR_SCOPE_PCT=50`) 08-03.  So "restrict kissat to our
# variant" now means approximately "plain kissat", and the scope framing no
# longer discriminates.  Under broad admission Factor DOES run on fclq (78
# factorings) and the instance still times out — so scope is no longer the
# blocker and the question has moved to what kissat's Factor DOES here.
#
# THE DECISIVE ARM is `kissat-no-factor`.  The cause-B filing predicts kissat
# loses fclq without Factor.  If instead kissat still solves it in ~16 s, then
# Factor is NOT kissat's lever on this instance, the cause-B filing is wrong,
# and the 12.6x from factorsize=2 was measuring something else (kissat's Factor
# restricted to binaries can be actively harmful rather than merely absent —
# `--no-factor` and `--factorsize=2` are NOT the same experiment, and the 07-30
# wave only ran the latter).  That distinction decides whether the remaining
# work is Factor quality or something else entirely.
#
# Reference: kissat solves fclq in 16.46-17.6 s across windows; all three of our
# arms time out at 300 s.  Instance shape: 205 vars / 4,549 clauses, 21.4% bin /
# 78.1% ternary, bin/var 4.76, occ/var 62.3 — tiny and DENSE.
set -u
R=$(git rev-parse --show-toplevel)
BIN=$R/build/pin-t3b/tactix
KISSAT=$R/tools/solvers/bin/kissat
INST=unsat-set-b-fclqcolor-10-07-09.sat05-1282.reshuffled-07.cnf.xz
CAP=300

arm="$1"
f=$(ls "$R"/benchmarks/cache/tierB/${INST%%.cnf.xz}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING instance for $INST"; exit 1; }
# $TMPDIR is shared per NODE, not per job — make the scratch name job-unique or
# two concurrent arms silently clobber each other (verified 2026-07-30).
cnf=$TMPDIR/fclq.$SLURM_JOB_ID.cnf
xz -dc "$f" > "$cnf"

run() { echo "=== arm=$arm $(date -Is)"; timeout $CAP "$@" "$cnf"; echo "=== exit $? arm=$arm"; }

case "$arm" in
  kissat)              run "$KISSAT" ;;
  kissat-no-factor)    run "$KISSAT" --no-factor ;;
  kissat-factorsize2)  run "$KISSAT" --factorsize=2 ;;
  ours-shipped)        run "$BIN" ;;
  ours-broad)          TACTIX_FACTOR_SCOPE_PCT=50 run "$BIN" ;;
  ours-broad-force)    TACTIX_FACTOR_SCOPE_PCT=50 TACTIX_FACTOR_FORCE=1 run "$BIN" ;;
  *) echo "UNKNOWN ARM $arm"; exit 1 ;;
esac
rm -f "$cnf"
