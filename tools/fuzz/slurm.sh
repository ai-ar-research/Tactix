#!/usr/bin/env bash
# Submit a fuzz leg as ONE multi-core SLURM job (see the slurm-jobs skill):
# partition cpu-dense-hwulab-q, 8,000 MiB/physical core, and one harness worker
# per physical core.  These SMT-2 nodes require two allocated logical CPUs to
# reserve one physical core.  Results are identical at any worker count — only
# wall-clock changes.
#
#   slurm.sh [--physical-cores P] [--time T] [--name NAME] [--wait] -- <run.sh args...>
#
# Examples (default: 50 physical cores, 100 logical CPUs, 50 workers):
#   slurm.sh --name quick   -- --quick   --solver build/debug/tactix
#   slurm.sh --name nightly -- --nightly --solver "build/release/tactix --random-policy --lrat"
#
# The job cd's to the repo root, so solver paths may be repo-relative.
# Log: tools/fuzz/slurm-<name>-<jobid>.log (same tail conventions as the
# nightly-*.log files). TMPDIR points at node-local /tmp for the per-case
# temp-dir churn; it is removed when the job exits.

set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

PHYSICAL_CORES=50 # cluster scale
THREADS_PER_CORE=2
MEM_MIB_PER_PHYSICAL_CORE=8000
TIME=24:00:00
NAME=fuzz
PARTITION=cpu-dense-hwulab-q
WAIT=0
while [ $# -gt 0 ]; do
    case "$1" in
        --physical-cores) PHYSICAL_CORES="$2"; shift 2 ;;
        --time) TIME="$2"; shift 2 ;;
        --name) NAME="$2"; shift 2 ;;
        --partition) PARTITION="$2"; shift 2 ;;
        --wait) WAIT=1; shift ;;
        --) shift; break ;;
        *) echo "usage: slurm.sh [--physical-cores P] [--time T] [--name NAME] -- <run.sh args>" >&2; exit 2 ;;
    esac
done
[ $# -gt 0 ] || { echo "error: no run.sh args after --" >&2; exit 2; }
case "$PHYSICAL_CORES" in
    ''|*[!0-9]*) echo "error: --physical-cores must be an integer" >&2; exit 2 ;;
esac
[ "$PHYSICAL_CORES" -ge 1 ] && [ "$PHYSICAL_CORES" -le 96 ] || {
    echo "error: --physical-cores must be between 1 and 96" >&2
    exit 2
}

ALLOC_CPUS=$((PHYSICAL_CORES * THREADS_PER_CORE))
MEM_MIB=$((PHYSICAL_CORES * MEM_MIB_PER_PHYSICAL_CORE))

# Re-quote the run.sh args so compound --solver strings survive the batch script.
ARGS=$(printf '%q ' "$@")

JID=$(sbatch --parsable --partition="$PARTITION" --nodes=1 --ntasks=1 \
       --cpus-per-task="$ALLOC_CPUS" --mem="${MEM_MIB}M" --time="$TIME" \
       --job-name="fuzz-$NAME" --output="$HERE/slurm-$NAME-%j.log" <<EOF
#!/usr/bin/env bash
set -euo pipefail
cd "$ROOT"
export TMPDIR=\$(mktemp -d /tmp/fuzz-\$SLURM_JOB_ID-XXXXXX)
trap 'rm -rf "\$TMPDIR"' EXIT
echo "== fuzz job \$SLURM_JOB_ID: node=\$SLURMD_NODENAME logical-cpus=\$SLURM_CPUS_PER_TASK physical-cores=$PHYSICAL_CORES workers=$PHYSICAL_CORES mem=${MEM_MIB}M =="
"$ROOT/tools/fuzz/run.sh" $ARGS --jobs "$PHYSICAL_CORES"
EOF
)
echo "submitted: $JID"
echo "log: $HERE/slurm-$NAME-$JID.log"
if [ "$WAIT" -eq 1 ]; then
    exec python3 "$ROOT/tools/slurm/wait.py" --job "$JID" \
         --log "$HERE/slurm-$NAME-$JID.log"
fi
