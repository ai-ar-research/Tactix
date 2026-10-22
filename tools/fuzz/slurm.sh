#!/usr/bin/env bash
# Submit a fuzz leg as one multi-core SLURM job or as a deterministic wide
# array (see the slurm-jobs skill).  The default cpu-dense-hwulab-q nodes use
# SMT-2, 8,000 MiB/physical core, and one harness worker per physical core.
# `--cpu-q-wide` instead splits one logical campaign over 50 array tasks of 10
# single-threaded workers: 500 eligible CPUs, the project-wide safety ceiling.
# Slurm places whatever the shared partition can admit.
# Results are identical at any worker/shard count; only wall-clock changes.
#
#   slurm.sh [--cpu-q-wide | --shards S] [--physical-cores P]
#            [--threads-per-core T] [--time T] [--name NAME]
#            [--checkers DIR] [--dsr-transport FILE] [--wait] -- <run.sh args...>
#
# Examples (default: 50 physical cores, 100 logical CPUs, 50 workers):
#   slurm.sh --name quick   -- --quick   --solver build/debug/tactix
#   slurm.sh --name nightly -- --nightly --solver "build/release/tactix --random-policy"
#   slurm.sh --name forced-ttf -- --forced-ttf --quick --solver build/debug/tactix
#
# The job cd's to the repo root, so solver paths may be repo-relative.
# --checkers DIR freezes the core clausal checkers: DIR holds exactly
# cake_lpr, drat-trim, dsr-compress, dsr-trim, lrat-check and srcheck plus a
# SHA256SUMS manifest.  Submission verifies it, installs a content-addressed
# read-only copy under failures/checkers/, and records that path and manifest
# in the snapshot; the job re-verifies it before running.  Without the flag the
# harness uses the mutable tools/checkers/bin links.
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
SHARDS=1
MEM_MIB_PER_PHYSICAL_CORE=8000
TIME=24:00:00
NAME=fuzz
PARTITION=cpu-dense-hwulab-q
WAIT=0
CHECKERS=""
DSR_TRANSPORT=""
while [ $# -gt 0 ]; do
    case "$1" in
        --physical-cores) PHYSICAL_CORES="$2"; shift 2 ;;
        --threads-per-core) THREADS_PER_CORE="$2"; shift 2 ;;
        --shards) SHARDS="$2"; shift 2 ;;
        --cpu-q-wide)
            PARTITION=cpu-q
            PHYSICAL_CORES=10
            THREADS_PER_CORE=1
            SHARDS=50
            shift
            ;;
        --time) TIME="$2"; shift 2 ;;
        --name) NAME="$2"; shift 2 ;;
        --partition) PARTITION="$2"; shift 2 ;;
        --checkers) CHECKERS="$2"; shift 2 ;;
        --dsr-transport) DSR_TRANSPORT="$2"; shift 2 ;;
        --wait) WAIT=1; shift ;;
        --) shift; break ;;
        *) echo "usage: slurm.sh [--cpu-q-wide | --shards S] [--physical-cores P] [--threads-per-core T] [--time T] [--name NAME] [--checkers DIR] [--dsr-transport FILE] -- <run.sh args>" >&2; exit 2 ;;
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
case "$THREADS_PER_CORE" in
    ''|*[!0-9]*) echo "error: --threads-per-core must be an integer" >&2; exit 2 ;;
esac
[ "$THREADS_PER_CORE" -ge 1 ] && [ "$THREADS_PER_CORE" -le 4 ] || {
    echo "error: --threads-per-core must be between 1 and 4" >&2
    exit 2
}
case "$SHARDS" in
    ''|*[!0-9]*) echo "error: --shards must be an integer" >&2; exit 2 ;;
esac
[ "$SHARDS" -ge 1 ] && [ "$SHARDS" -le 1024 ] || {
    echo "error: --shards must be between 1 and 1024" >&2
    exit 2
}

RUN_ARGS=("$@")
TOTAL_CASES=10000
if [ "$SHARDS" -gt 1 ]; then
    for ((arg_index = 0; arg_index < ${#RUN_ARGS[@]}; ++arg_index)); do
        case "${RUN_ARGS[$arg_index]}" in
            --quick) TOTAL_CASES=10000 ;;
            --nightly) TOTAL_CASES=200000 ;;
            --n)
                arg_index=$((arg_index + 1))
                [ "$arg_index" -lt "${#RUN_ARGS[@]}" ] || {
                    echo "error: --n is missing its value" >&2
                    exit 2
                }
                TOTAL_CASES="${RUN_ARGS[$arg_index]}"
                ;;
            --outdir)
                echo "error: sharded fuzz owns per-task output directories; do not pass --outdir" >&2
                exit 2
                ;;
        esac
    done
    case "$TOTAL_CASES" in
        ''|*[!0-9]*) echo "error: fuzz case count must be an integer" >&2; exit 2 ;;
    esac
    [ "$TOTAL_CASES" -ge "$SHARDS" ] || {
        echo "error: fuzz case count must be at least the shard count" >&2
        exit 2
    }
fi
# NAME becomes both a log filename component and the private artifact leg.
# Reject shell/path punctuation before interpolating it into the batch script.
python3 "$HERE/artifact_layout.py" "$HERE/failures" validation "$NAME" \
    >/dev/null

ALLOC_CPUS=$((PHYSICAL_CORES * THREADS_PER_CORE))
MEM_MIB=$((PHYSICAL_CORES * MEM_MIB_PER_PHYSICAL_CORE))
if [ "$PARTITION" = cpu-q ]; then
    [ "$ALLOC_CPUS" -le 52 ] || {
        echo "error: cpu-q permits at most 52 CPUs per node/task" >&2
        exit 2
    }
    [ "$((ALLOC_CPUS * SHARDS))" -le 500 ] || {
        echo "error: cpu-q fuzz submissions may request at most 500 total cores" >&2
        exit 2
    }
fi

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
cleanup_unsubmitted_pin() {
    chmod -R u+w "$PIN_ROOT" 2>/dev/null || true
    rm -rf "$PIN_ROOT"
}
trap cleanup_unsubmitted_pin EXIT
PIN_FUZZ="$PIN_ROOT/fuzz"
mkdir -p "$PIN_FUZZ"
cp -a "$HERE/run.sh" "$HERE/fuzz.py" "$HERE/generators.py" \
      "$HERE/modelcheck.py" "$HERE/shrink.py" "$HERE/stress.py" \
      "$HERE/controller_v5.py" "$HERE/forced_ttf.py" \
      "$HERE/artifact_layout.py" "$HERE/shard.py" "$HERE/aggregate.py" \
      "$HERE/corpus" \
      "$HERE/stress-scenarios" "$PIN_FUZZ/"
mkdir -p "$PIN_FUZZ/controls"
cp -a "$HERE/controls/controller-v5" "$PIN_FUZZ/controls/"
cp -a "$HERE/controls/forced-ttf" "$PIN_FUZZ/controls/"
cp -a "$ROOT/tools/bench/proof_oracle.py" "$PIN_FUZZ/"
if [ -n "$DSR_TRANSPORT" ]; then
    python3 - "$ROOT/tools/bench" "$DSR_TRANSPORT" "$PIN_FUZZ" <<'PYTRANSPORT'
import sys
sys.path.insert(0, sys.argv[1])
from proof_oracle import pin_dsr_transport
pin_dsr_transport(sys.argv[2], sys.argv[3])
PYTRANSPORT
fi
mkdir -p "$PIN_FUZZ/checkers"
cp -aL "$ROOT/tools/checkers/bin/veripb" "$PIN_FUZZ/checkers/veripb"
cp -aL "$ROOT/tools/checkers/bin/cake-pb-cnf" \
      "$PIN_FUZZ/checkers/cake-pb-cnf"
CHECKERS_ROOT=""
if [ -n "$CHECKERS" ]; then
    CHECKERS="$(cd "$CHECKERS" && pwd)"
    LISTED=$(awk '{print $2}' "$CHECKERS/SHA256SUMS" | LC_ALL=C sort | tr '\n' ' ')
    [ "$LISTED" = "cake_lpr drat-trim dsr-compress dsr-trim lrat-check srcheck " ] || {
        echo "error: $CHECKERS/SHA256SUMS must list exactly the six core checkers" >&2
        exit 2
    }
    (cd "$CHECKERS" && sha256sum --strict --quiet -c SHA256SUMS) || {
        echo "error: $CHECKERS does not match its SHA256SUMS" >&2
        exit 2
    }
    CHECKERS_SHA=$(sha256sum < "$CHECKERS/SHA256SUMS" | cut -d' ' -f1)
    CHECKERS_PARENT="$HERE/failures/checkers"
    CHECKERS_ROOT="$CHECKERS_PARENT/$CHECKERS_SHA"
    mkdir -p "$CHECKERS_PARENT"
    if [ ! -d "$CHECKERS_ROOT" ]; then
        CHECKERS_TMP=$(mktemp -d "$CHECKERS_PARENT/.install-XXXXXX")
        for name in cake_lpr drat-trim dsr-compress dsr-trim lrat-check srcheck; do
            cp -L "$CHECKERS/$name" "$CHECKERS_TMP/$name"
        done
        cp "$CHECKERS/SHA256SUMS" "$CHECKERS_TMP/"
        [ ! -f "$CHECKERS/PROVENANCE.json" ] ||
            cp "$CHECKERS/PROVENANCE.json" "$CHECKERS_TMP/"
        (cd "$CHECKERS_TMP" && sha256sum --strict --quiet -c SHA256SUMS)
        chmod -R a-w "$CHECKERS_TMP"
        if ! mv -T "$CHECKERS_TMP" "$CHECKERS_ROOT" 2>/dev/null; then
            chmod -R u+w "$CHECKERS_TMP"
            rm -rf "$CHECKERS_TMP"
        fi
    fi
    (cd "$CHECKERS_ROOT" && sha256sum --strict --quiet -c "$CHECKERS/SHA256SUMS") || {
        echo "error: installed checkers $CHECKERS_ROOT do not match $CHECKERS" >&2
        exit 2
    }
    printf '%s\n' "$CHECKERS_ROOT" > "$PIN_FUZZ/core-checkers"
    cp "$CHECKERS/SHA256SUMS" "$PIN_FUZZ/core-checkers.sha256"
fi
HARNESS_SHA=$(
    cd "$PIN_FUZZ"
    find . -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum | \
        sha256sum | cut -d' ' -f1
)
chmod -R a-w "$PIN_ROOT"

# Retained non-terminating controls outlive a successful batch job, while the
# transient build/pin snapshot deliberately does not.  Keep one small,
# content-addressed replay harness per distinct hash; concurrent submissions
# converge on the same immutable directory.
REPLAY_PARENT="$HERE/failures/harnesses"
REPLAY_ROOT="$REPLAY_PARENT/$HARNESS_SHA"
mkdir -p "$REPLAY_PARENT"
if [ ! -d "$REPLAY_ROOT/fuzz" ]; then
    REPLAY_TMP=$(mktemp -d "$REPLAY_PARENT/.install-XXXXXX")
    cp -a "$PIN_FUZZ" "$REPLAY_TMP/fuzz"
    chmod -R a-w "$REPLAY_TMP"
    if ! mv -T "$REPLAY_TMP" "$REPLAY_ROOT" 2>/dev/null; then
        chmod -R u+w "$REPLAY_TMP"
        rm -rf "$REPLAY_TMP"
        [ -d "$REPLAY_ROOT/fuzz" ] || {
            echo "error: failed to install replay harness $REPLAY_ROOT" >&2
            exit 2
        }
    fi
fi

if [ "$SHARDS" -eq 1 ]; then
    JID=$(sbatch --parsable --partition="$PARTITION" --nodes=1 --ntasks=1 \
           --cpus-per-task="$ALLOC_CPUS" --mem="${MEM_MIB}M" --time="$TIME" \
           --job-name="fuzz-$NAME" --output="$HERE/slurm-$NAME-%j.log" <<EOF
#!/usr/bin/env bash
set -euo pipefail
cd "$ROOT"
export TMPDIR=\$(mktemp -d /tmp/fuzz-\$SLURM_JOB_ID-XXXXXX)
export TACTIX_REPO_ROOT="$ROOT"
export TACTIX_FUZZ_OUTPUT_ROOT="$HERE/failures"
export TACTIX_FUZZ_RUN_ID="slurm-\$SLURM_JOB_ID"
export TACTIX_FUZZ_LEG_ID="$NAME"
export TACTIX_FUZZ_REPLAY_HARNESS="$REPLAY_ROOT/fuzz"
export TACTIX_FUZZ_HARNESS_SHA256="$HARNESS_SHA"
if [ -f "$PIN_FUZZ/core-checkers" ]; then
    (cd "$CHECKERS_ROOT" && sha256sum --strict -c "$PIN_FUZZ/core-checkers.sha256")
fi
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
echo "== fuzz job \$SLURM_JOB_ID: node=\$SLURMD_NODENAME logical-cpus=\$SLURM_CPUS_PER_TASK physical-cores=$PHYSICAL_CORES workers=$PHYSICAL_CORES mem=${MEM_MIB}M harness-sha256=$HARNESS_SHA core-checkers=${CHECKERS_ROOT:-tools/checkers/bin} =="
"$PIN_FUZZ/run.sh" $ARGS --jobs "$PHYSICAL_CORES"
EOF
    )
    trap - EXIT
    echo "submitted: $JID"
    echo "log: $HERE/slurm-$NAME-$JID.log"
    echo "harness: $HARNESS_SHA ($PIN_ROOT)"
    echo "core checkers: ${CHECKERS_ROOT:-tools/checkers/bin (unfrozen)}"
    if [ "$WAIT" -eq 1 ]; then
        exec python3 "$ROOT/tools/slurm/wait.py" --job "$JID" \
             --log "$HERE/slurm-$NAME-$JID.log"
    fi
    exit 0
fi

ARRAY_LAST=$((SHARDS - 1))
JID=$(sbatch --parsable --partition="$PARTITION" --nodes=1 --ntasks=1 \
       --cpus-per-task="$ALLOC_CPUS" --mem="${MEM_MIB}M" --time="$TIME" \
       --array="0-$ARRAY_LAST" --job-name="fuzz-$NAME" \
       --output="$HERE/slurm-$NAME-%A_%a.log" <<EOF
#!/usr/bin/env bash
set -euo pipefail
cd "$ROOT"
TASK_ID=\${SLURM_ARRAY_TASK_ID:?missing array task id}
read -r INDEX_OFFSET CASE_COUNT < <(
    python3 "$PIN_FUZZ/shard.py" "$TOTAL_CASES" "$SHARDS" "\$TASK_ID"
)
export TMPDIR=\$(mktemp -d /tmp/fuzz-\${SLURM_ARRAY_JOB_ID}-\${TASK_ID}-XXXXXX)
export TACTIX_REPO_ROOT="$ROOT"
export TACTIX_FUZZ_OUTPUT_ROOT="$HERE/failures"
export TACTIX_FUZZ_RUN_ID="slurm-\$SLURM_ARRAY_JOB_ID"
export TACTIX_FUZZ_LEG_ID="$NAME-shard-\$TASK_ID"
export TACTIX_FUZZ_REPLAY_HARNESS="$REPLAY_ROOT/fuzz"
export TACTIX_FUZZ_HARNESS_SHA256="$HARNESS_SHA"
STATE_ROOT="$HERE/failures/runs/slurm-\$SLURM_ARRAY_JOB_ID/.array-$NAME"
if [ -f "$PIN_FUZZ/core-checkers" ]; then
    (cd "$CHECKERS_ROOT" && sha256sum --strict -c "$PIN_FUZZ/core-checkers.sha256")
fi
cleanup_fuzz_shard() {
    status=\$?
    trap - EXIT
    rm -rf "\$TMPDIR"
    mkdir -p "\$STATE_ROOT" || status=1
    marker="\$STATE_ROOT/\${TASK_ID}.status.tmp.\$\$"
    if ! printf '%s\n' "\$status" > "\$marker" ||
       ! mv "\$marker" "\$STATE_ROOT/\${TASK_ID}.status"; then
        status=1
    fi
    exit "\$status"
}
trap cleanup_fuzz_shard EXIT
SHARD_ARGS=()
if [ "\$TASK_ID" -ne 0 ]; then
    SHARD_ARGS+=(--skip-corpus --skip-fixed-controls)
fi
echo "== fuzz array \$SLURM_ARRAY_JOB_ID task \$TASK_ID: node=\$SLURMD_NODENAME cpus=\$SLURM_CPUS_PER_TASK workers=$PHYSICAL_CORES cases=\$CASE_COUNT offset=\$INDEX_OFFSET total=$TOTAL_CASES shards=$SHARDS mem=${MEM_MIB}M harness-sha256=$HARNESS_SHA core-checkers=${CHECKERS_ROOT:-tools/checkers/bin} =="
"$PIN_FUZZ/run.sh" $ARGS --n "\$CASE_COUNT" \
    --index-offset "\$INDEX_OFFSET" "\${SHARD_ARGS[@]}" \
    --defer-requirements \
    --jobs "$PHYSICAL_CORES"
EOF
)
trap - EXIT

STATE_ROOT="$HERE/failures/runs/slurm-$JID/.array-$NAME"
FINAL_LOG="$HERE/slurm-$NAME-finalize-%j.log"
FINAL_JID=$(sbatch --parsable --partition="$PARTITION" --nodes=1 --ntasks=1 \
       --cpus-per-task=1 --mem=2000M --time=00:10:00 \
       --dependency="afterany:$JID" --job-name="fuzz-$NAME-finalize" \
       --output="$FINAL_LOG" <<EOF
#!/usr/bin/env bash
set -euo pipefail
failed=0
for ((task = 0; task < $SHARDS; ++task)); do
    marker="$STATE_ROOT/\${task}.status"
    if [ ! -f "\$marker" ]; then
        echo "missing shard status: \$task"
        failed=1
        continue
    fi
    status=\$(<"\$marker")
    if [ "\$status" -ne 0 ]; then
        echo "failed shard: \$task exit=\$status"
        failed=1
    fi
done
if [ "\$failed" -ne 0 ]; then
    echo "retained incomplete/failed array harness pin: $PIN_ROOT"
    exit 1
fi
python3 "$PIN_FUZZ/aggregate.py" \
    --root "$HERE/failures/runs/slurm-$JID" \
    --prefix "$NAME-shard-" --shards "$SHARDS" --total "$TOTAL_CASES" || {
    echo "retained array harness pin after aggregate qualification failure: $PIN_ROOT"
    exit 1
}
chmod -R u+w "$PIN_ROOT"
rm -rf "$PIN_ROOT"
echo "fuzz array PASS: $SHARDS/$SHARDS shards, $TOTAL_CASES deterministic cases"
EOF
)

echo "submitted array: $JID (0-$ARRAY_LAST, $SHARDS shards)"
echo "array logs: $HERE/slurm-$NAME-${JID}_<task>.log"
echo "submitted finalizer: $FINAL_JID"
echo "finalizer log: $HERE/slurm-$NAME-finalize-$FINAL_JID.log"
echo "resources: partition=$PARTITION workers-per-shard=$PHYSICAL_CORES logical-cpus-per-shard=$ALLOC_CPUS total-eligible-workers=$((PHYSICAL_CORES * SHARDS))"
echo "harness: $HARNESS_SHA ($PIN_ROOT)"
echo "core checkers: ${CHECKERS_ROOT:-tools/checkers/bin (unfrozen)}"
if [ "$WAIT" -eq 1 ]; then
    exec python3 "$ROOT/tools/slurm/wait.py" --job "$FINAL_JID" \
         --log "$HERE/slurm-$NAME-finalize-$FINAL_JID.log"
fi
