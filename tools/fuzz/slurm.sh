#!/usr/bin/env bash
# Submit a fuzz leg as ONE multi-core SLURM job (cluster protocol,
# PROGRESS Log 2026-07-23): partition cpu-dense-hwulab-q, 8 GB/core,
# harness --jobs pinned to the allocation. Results are identical at any
# job count (fuzz skill) — only wall-clock changes.
#
#   slurm.sh [--cpus C] [--time T] [--name NAME] -- <run.sh args...>
#
# Examples:
#   slurm.sh --cpus 16 --name quick   -- --quick   --solver build/debug/tactix
#   slurm.sh --cpus 32 --name nightly -- --nightly --solver "build/release/tactix --random-policy --lrat"
#
# The job cd's to the repo root, so solver paths may be repo-relative.
# Log: tools/fuzz/slurm-<name>-<jobid>.log (same tail conventions as the
# nightly-*.log files). TMPDIR points at node-local /tmp for the per-case
# temp-dir churn; it is removed when the job exits.

set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

CPUS=16
TIME=24:00:00
NAME=fuzz
PARTITION=cpu-dense-hwulab-q
while [ $# -gt 0 ]; do
    case "$1" in
        --cpus) CPUS="$2"; shift 2 ;;
        --time) TIME="$2"; shift 2 ;;
        --name) NAME="$2"; shift 2 ;;
        --partition) PARTITION="$2"; shift 2 ;;
        --) shift; break ;;
        *) echo "usage: slurm.sh [--cpus C] [--time T] [--name NAME] -- <run.sh args>" >&2; exit 2 ;;
    esac
done
[ $# -gt 0 ] || { echo "error: no run.sh args after --" >&2; exit 2; }

# Re-quote the run.sh args so compound --solver strings survive the batch script.
ARGS=$(printf '%q ' "$@")

sbatch --partition="$PARTITION" --nodes=1 --ntasks=1 \
       --cpus-per-task="$CPUS" --mem-per-cpu=8G --time="$TIME" \
       --job-name="fuzz-$NAME" --output="$HERE/slurm-$NAME-%j.log" <<EOF
#!/usr/bin/env bash
set -euo pipefail
cd "$ROOT"
export TMPDIR=\$(mktemp -d /tmp/fuzz-\$SLURM_JOB_ID-XXXXXX)
trap 'rm -rf "\$TMPDIR"' EXIT
echo "== fuzz job \$SLURM_JOB_ID: node=\$SLURMD_NODENAME cpus=\$SLURM_CPUS_PER_TASK mem/cpu=8G =="
"$ROOT/tools/fuzz/run.sh" $ARGS --jobs "\$SLURM_CPUS_PER_TASK"
EOF
