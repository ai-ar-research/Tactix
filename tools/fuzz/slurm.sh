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
# Submission snapshots every harness input used by run.sh.  The batch job never
# executes a live repository script, so later edits cannot shift Bash's saved
# file offset while a long-running child process is active.
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

# Pin only the inputs used by run.sh.  In particular, do not copy mutable logs,
# failure output, or Python caches.  A failed job retains this exact read-only
# snapshot for direct diagnosis; a successful job removes it.
mkdir -p "$ROOT/build"
PIN_ROOT=$(mktemp -d "$ROOT/build/pin-fuzz-harness-XXXXXX")
case "$PIN_ROOT" in
    "$ROOT"/build/pin-fuzz-harness-*) ;;
    *) echo "error: unsafe fuzz harness pin path: $PIN_ROOT" >&2; exit 2 ;;
esac
cleanup_unsubmitted_pin() { rm -rf "$PIN_ROOT"; }
trap cleanup_unsubmitted_pin EXIT
PIN_FUZZ="$PIN_ROOT/fuzz"
mkdir -p "$PIN_FUZZ"
cp -a "$HERE/run.sh" "$HERE/fuzz.py" "$HERE/generators.py" \
      "$HERE/modelcheck.py" "$HERE/shrink.py" "$HERE/stress.py" \
      "$HERE/corpus" "$HERE/stress-scenarios" "$PIN_FUZZ/"
HARNESS_SHA=$(
    cd "$PIN_FUZZ"
    find . -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum | \
        sha256sum | cut -d' ' -f1
)
chmod -R a-w "$PIN_ROOT"

JID=$(sbatch --parsable --partition="$PARTITION" --nodes=1 --ntasks=1 \
       --cpus-per-task="$ALLOC_CPUS" --mem="${MEM_MIB}M" --time="$TIME" \
       --job-name="fuzz-$NAME" --output="$HERE/slurm-$NAME-%j.log" <<EOF
#!/usr/bin/env bash
set -euo pipefail
cd "$ROOT"
export TMPDIR=\$(mktemp -d /tmp/fuzz-\$SLURM_JOB_ID-XXXXXX)
export TACTIX_REPO_ROOT="$ROOT"
export TACTIX_FUZZ_OUTPUT_ROOT="$HERE/failures"
cleanup_fuzz_job() {
    status=\$?
    rm -rf "\$TMPDIR"
    if [ "\$status" -eq 0 ]; then
        # The snapshot is read-only including its directories, so restore
        # owner write permission first: rm -rf forces read-only files but
        # cannot unlink entries from an unwritable directory.
        chmod -R u+w "$PIN_ROOT"
        rm -rf "$PIN_ROOT"
    else
        echo "retained failed-job harness pin: $PIN_ROOT"
    fi
}
trap cleanup_fuzz_job EXIT
echo "== fuzz job \$SLURM_JOB_ID: node=\$SLURMD_NODENAME logical-cpus=\$SLURM_CPUS_PER_TASK physical-cores=$PHYSICAL_CORES workers=$PHYSICAL_CORES mem=${MEM_MIB}M harness-sha256=$HARNESS_SHA =="
"$PIN_FUZZ/run.sh" $ARGS --jobs "$PHYSICAL_CORES"
EOF
)
trap - EXIT
echo "submitted: $JID"
echo "log: $HERE/slurm-$NAME-$JID.log"
echo "harness: $HARNESS_SHA ($PIN_ROOT)"
if [ "$WAIT" -eq 1 ]; then
    exec python3 "$ROOT/tools/slurm/wait.py" --job "$JID" \
         --log "$HERE/slurm-$NAME-$JID.log"
fi
