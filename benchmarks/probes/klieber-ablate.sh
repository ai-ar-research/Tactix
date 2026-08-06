#!/usr/bin/env bash
# DIAGNOSIS ONLY.  Ladder step 3/4 for `mp1-klieber2017s-0500-023-t12`: rule out
# a MISSING RULE before blaming heuristics, by ablating the REFERENCE.
#
# Baseline reading (job 1585931): kissat 12.3 s / 64,149 conflicts, cadical
# 61.9 s / 357,783 conflicts, both ~5-6 k conflicts/s.  So between the two
# references the entire gap is search-tree SIZE, not engine speed — which makes
# "which simplification shrinks the tree here" the question.
#
# Instance shape (from the CNF, no run needed): 30,587 vars / 91,703 clauses =
# 1 unit + 61,138 binaries + 30,563 ternaries + one 13-clause.  2.0 binaries per
# var and ~one ternary per var: a gate-definition circuit.  Factor's density
# gate needs >= 6 binaries/var, so Factor is OFF here by construction.
#
# Manifest line: <label>|<solver>|<flag>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving
W=$R/.claude/worktrees/agent-aaf1646d7c94f106d
STEM=mp1-klieber2017s-0500-023-t12
TIER=tierB

IFS='|' read -r label solver flag cap <<<"$1"

case "$solver" in
  kissat)  BIN=$R/tools/solvers/bin/kissat ;;
  cadical) BIN=$R/tools/solvers/bin/cadical ;;
  tactix)  BIN=$W/build/pin-dbgk/tactix ;;
  *) echo "UNKNOWN SOLVER $solver"; exit 1 ;;
esac
[ -x "$BIN" ] || { echo "MISSING BINARY $BIN"; exit 1; }

f=$(ls "$R"/benchmarks/cache/$TIER/${STEM}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE"; exit 1; }
work=${TMPDIR:-/tmp}/klbab-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work" || exit 1
cnf=$work/$STEM.cnf
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; rm -rf "$work"; exit 1; }

echo "=== label=$label solver=$solver flag=$flag cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is) bin-sha $(sha256sum "$BIN" | cut -c1-16)"

args=()
if [ "$flag" != "none" ]; then
  # semicolon-separated so one task can carry several flags/env assignments
  IFS=';' read -r -a parts <<<"$flag"
  for p in "${parts[@]}"; do
    case "$p" in
      -*) args+=("$p") ;;
      *=*) export "${p?}" ;;
      *) echo "UNKNOWN FLAG PART $p"; rm -rf "$work"; exit 1 ;;
    esac
  done
fi
echo "=== argv: ${args[*]:-<none>}  env-extra: $flag"
[ "$solver" = tactix ] && export TACTIX_SLOT_STATS=1

/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" \
  "$BIN" "${args[@]}" "$cnf" 2>&1 \
  | grep -E '^s |conflicts:|decisions:|propagations:|restarts:|reductions:|walked:|^c conflicts|^c decisions|^c propagations|^c restarts|^c walk|^c inprocess|^c slot |^c sweep|^c congruence|^c congy1|^c vivify2|^c bve |^c factor |^wall |maxrss'

echo "=== done $(date -Is)"
rm -rf "$work"
