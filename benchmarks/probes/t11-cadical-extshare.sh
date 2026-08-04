#!/usr/bin/env bash
# Task #11, increment 2 (L14 compliance): what share of CADICAL's decisions land
# on ITS factor-added extension variables, on the SAME instances?
#
# WHY: the 2026-07-31 diagnosis calls our 31.4 % ext-decision share on
# ContextModel "the single clearest anomaly in the whole investigation" -- but
# that is an INTERNAL cross-instance contrast (31.4 % vs our own 0.38 % / 6.2 %
# elsewhere), which is exactly the shape L14 warns about: our minimization
# "defect" on FmlaEquiv dissolved once kissat's number on the SAME instance was
# measured (22.1 % vs our 21.6 % = parity all along).  Nobody has ever measured
# cadical's share.  Two possible readings, both decisive:
#   * cadical's share is LOW (<< 31 %)  -> ext-var branching really is our
#     anomaly, and adaptive/persistent ext priority is the right (novel) lever.
#   * cadical's share is COMPARABLE     -> ext branching is NOT the mechanism,
#     our 3.7 % no-ext ablation was an L8-style single-ablation artefact, and
#     task #11's remaining premise collapses too.
#
# BINARY: tools/solvers/variants/cadical-extdiag -- a patched copy built outside
# the PINNED CLONE, as L14 requires (tools/solvers/bin/* are SYMLINKS into the
# reference clones, so building a variant in place would silently replace the
# pinned baseline that every tier run measures against).  It lived in $HOME until
# the 2026-08-03 re-org; variants now live under tools/solvers/variants/, which
# satisfies the same constraint without scattering the project across $HOME.
# Same version as the baseline: 3.0.0.
# Patch (marked EXTDIAG): stats.orig_max_var snapshotted at Internal::solve
# entry, stats.decisions_extension incremented in decide() when the chosen idx
# exceeds it, printed by print_stats as "extension: N  P % of decisions".
# NOTE: cadical must be configured WITHOUT -q -- that flag means -DQUIET and
# compiles the statistics out entirely (same trap as L14's kissat METRIC tier).
#
# Manifest line: <label>|<tier>|<stem>|<cap-seconds>
set -u
R=/home/hwu/SAT-solving
BIN=$(git rev-parse --show-toplevel)/tools/solvers/variants/cadical-extdiag/build/cadical

line="$1"
IFS='|' read -r label tier stem cap <<<"$line"

f=$(ls "$R"/benchmarks/cache/$tier/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING INSTANCE $stem in $tier"; exit 1; }
work=$TMPDIR/t11ext-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work"
cnf=$work/$(basename "$f" .cnf.xz).cnf
xz -dc "$f" >"$cnf" || { echo "STAGE FAILED"; exit 1; }

echo "=== label=$label instance=$stem tier=$tier cap=${cap}s"
echo "=== host=$(hostname) date=$(date -Is) bin=$BIN"
grep -m1 '^p cnf' "$cnf"

/usr/bin/time -f "wall %e s   maxrss %M KB" timeout "$cap" "$BIN" --stats=true "$cnf" 2>&1 \
  | grep -v '^v '

echo "=== done $(date -Is)"
rm -rf "$work"
