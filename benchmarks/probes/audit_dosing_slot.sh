#!/bin/bash
# Dosing-governor audit probe (branch audit-dosing, 2026-08-06).
#
# Runs tactix with the per-entry slot ledger on (TACTIX_SLOT_STATS=1) and keeps
# the counter lines. COUNTERS ONLY: no timing claim is made from this probe, so
# it is deliberately unthrottled and safe to run while other agents work.
#
#   sbatch --array=0-N ... audit_dosing_slot.sh <bindir> <outdir> [VAR=val ...]
#
# The instance list is benchmarks/probes/audit_dosing_instances.txt (paths
# relative to benchmarks/cache/).
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
# The instance cache is gitignored, so a WORKTREE does not have one: fall back to
# the main checkout's cache. CACHE=<dir> overrides.
CACHE=${CACHE:-$ROOT/benchmarks/cache}
if [ ! -d "$CACHE" ]; then
  CACHE=$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir)/../benchmarks/cache
fi
BIN=$1
OUT=$2
shift 2
mapfile -t INSTS < "$ROOT/benchmarks/probes/audit_dosing_instances.txt"
INST=${INSTS[$SLURM_ARRAY_TASK_ID]}
name=$(basename "$INST" .cnf.xz)
# $TMPDIR is shared per NODE, not per job — the name must be job-unique.
cnf=$TMPDIR/$name.$SLURM_JOB_ID.$SLURM_ARRAY_TASK_ID.cnf
xz -dc "$CACHE/$INST" > "$cnf" || exit 3
# A probe that reads an empty CNF returns "s SATISFIABLE / v 0" and looks
# plausible; refuse instead (slurm-jobs: sanity-check every probe's input).
[ -s "$cnf" ] || exit 3
tag=${TAG:-base}
log=$OUT/$name.$tag.log
{
  echo "# instance $INST"
  echo "# extra-env $*"
  env "$@" TACTIX_SLOT_STATS=1 timeout 300 "$BIN/tactix" "$cnf"
  echo "# exit $?"
} > "$log" 2>&1
rm -f "$cnf"
