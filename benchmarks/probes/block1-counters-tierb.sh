#!/usr/bin/env bash
# Submit block1-counters.sh across the WHOLE of Tier B as one array.
#
#   benchmarks/probes/block1-counters-tierb.sh <build-dir-name> [cap]
#
# THE INVARIANT THIS EXISTS TO ENFORCE: sample the whole tier, never a subset.
# Both counters this probe reads — D3's `blocked` and E3's `clamped` — are zero
# on every instance that finishes quickly and non-zero only on the ones that run
# to the cap, because both are driven by the cumulative clock. A subsample that
# happens to exclude the long runs therefore reads a clean zero for a reason that
# has nothing to do with the arm, and retiring the arm on it banks a vacuous
# negative (the shape [[ab-arm-identical-artefacts]] and the `F2` row describe).
# An 8-instance subsample of this tier topped out an order of magnitude below the
# instances that decide both counters, and retired two arms that the full tier
# then put back.
#
# Counters only: no timing claim is readable here, so the partition need not be
# quiet. Neither arm is set — see block1-counters.sh for the pre-registration.
set -eu
R=$(git rev-parse --show-toplevel)
build=${1:?usage: block1-counters-tierb.sh <build-dir-name> [cap]}
cap=${2:-300}
[ -x "$R/build/$build/tactix" ] || { echo "no binary at build/$build/tactix"; exit 1; }

D=$R/benchmarks/probes/scratch/block1-d3
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
D=$R/benchmarks/probes/scratch/block1-d3
arg=$(sed -n "${SLURM_ARRAY_TASK_ID}p" "$D/stems.txt")
[ -n "$arg" ] || { echo "NO STEM for task ${SLURM_ARRAY_TASK_ID}"; exit 1; }
exec "$R/benchmarks/probes/block1-counters.sh" "$arg"
EOF
chmod +x "$D/task.sh"

rm -f "$D"/*_*.out
jid=$(sbatch --parsable --partition=cpu-dense-hwulab-q --ntasks=1 --cpus-per-task=1 \
  --mem-per-cpu=8G --time=00:12:00 --job-name=blk1d3 --array=1-"$n" \
  --output="$D/%A_%a.out" "$D/task.sh")
python3 "$R/tools/bench/buildlock.py" --acquire "build/$build" --job "$jid"
echo "submitted $jid ($n tasks); summarize with benchmarks/probes/block1-summarize.py"
