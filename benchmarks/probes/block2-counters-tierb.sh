#!/usr/bin/env bash
# Submit the block-2 ANTI-VACUITY readout across the whole of Tier B as one array.
#
#   benchmarks/probes/block2-counters-tierb.sh <build-dir-name> [cap]
#
# WHAT IT IS FOR. Every row of the phase-4 pricing table names a source counter
# that must be checked FIRST: an arm whose counter did not move was not tested,
# it was skipped ([[ab-arm-identical-artefacts]]). run.py DISCARDS solver stdout
# and stores only the `reached` map, so a pricing window cannot answer that
# question about itself. This probe answers it for EVERY row at once, before the
# window is spent, so a mechanism that never fires is dropped from the batch
# instead of buying an arm that can only measure noise.
#
# It keeps the WHOLE `c ` block per instance rather than a grep list, so a row
# added to the table later is read off the same artefact with no re-run.
# TACTIX_SLOT_STATS is a print switch, not an arm: `c slot`, `c slot-clock`,
# `c slot-order`, `c cand-order` and `c meters` come from report_slot
# (default_policy.cpp) and print only under it; every other `c ` line comes from
# main.cpp's print_stats and prints unconditionally.
#
# NO ARM IS SET. This is the shipped configuration's own reach, which is what
# decides whether an OFF arm can differ from it at all.
#
# SAMPLE THE WHOLE TIER, NEVER A SUBSET. Counters driven by the cumulative clock
# read a clean zero on every instance that finishes quickly, so a subsample that
# misses the long runs retires an arm for a reason that has nothing to do with
# the arm — this cost two retirements on 2026-08-07 that the full tier put back.
#
# Counters only: no timing claim is readable here, so the partition need not be
# quiet.
set -eu
R=$(git rev-parse --show-toplevel)
build=${1:?usage: block2-counters-tierb.sh <build-dir-name> [cap]}
cap=${2:-300}
[ -x "$R/build/$build/tactix" ] || { echo "no binary at build/$build/tactix"; exit 1; }

D=$R/benchmarks/probes/scratch/block2-counters
mkdir -p "$D"
awk '!/^#/ && NF {print $1}' "$R/benchmarks/tiers/B.list" \
  | sed 's|^tierB/||; s|\.cnf\.xz$||' \
  | awk -v b="$build" -v c="$cap" '{print $0"|tierB|"b"|"c}' > "$D/stems.txt"
n=$(wc -l < "$D/stems.txt")

# Resolve paths from the JOB's working directory, never from $0: SLURM copies the
# batch script into its own spool dir, so `dirname $0` points there.
cat > "$D/task.sh" <<'EOF'
#!/usr/bin/env bash
set -u
R=$(git rev-parse --show-toplevel)
D=$R/benchmarks/probes/scratch/block2-counters
arg=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$D/stems.txt")
[ -n "$arg" ] || { echo "NO STEM for task ${SLURM_ARRAY_TASK_ID}"; exit 1; }
exec "$R/benchmarks/probes/block2-counters.sh" "$arg"
EOF
chmod +x "$D/task.sh"

rm -f "$D"/*_*.out
jid=$(sbatch --parsable --partition=cpu-dense-hwulab-q --ntasks=1 --cpus-per-task=1 \
  --mem-per-cpu=8G --time=00:12:00 --job-name=blk2cnt --array=1-"$n" \
  --output="$D/%A_%a.out" "$D/task.sh")
python3 "$R/tools/bench/buildlock.py" --acquire "build/$build" --job "$jid"
echo "submitted $jid ($n tasks); summarize with benchmarks/probes/block2-summarize.py"
