#!/usr/bin/env bash
# RV-S31: measure the --random-policy non-termination population, which is
# silently eating random-policy fuzz coverage (a case that never reaches a
# verdict runs none of the semantic oracles, so it is lost coverage that reads
# as green).
#
# Three legs at one timeout, same generator seeds, one binary:
#   rp-drat    --random-policy=7
#   rp-lrat    --random-policy=7 --lrat   (both proof formats, per the finding)
#   def-drat   DefaultPolicy               (the control: expected 0)
# then an ESCALATION pass: every non-terminating case is re-run at a much
# larger timeout, which is what separates "slow under a random schedule" from
# "does not terminate". The first number alone is only a floor.
#
# No timing claim is derived here — only terminates / does not terminate — so
# this job does not need an idle partition. Submit with:
#   sbatch --partition=cpu-dense-hwulab-q --nodes=1 --ntasks=1 \
#          --cpus-per-task=50 --mem-per-cpu=8G --time=06:00:00 \
#          --job-name=rvs31 --output=<log> benchmarks/probes/rv-s31-livelock.sh
set -uo pipefail
# sbatch copies the script to the node's spool dir, so $0 says nothing about the
# repo: start from the submit dir (or the script's own path when run by hand)
# and let git name the root, per CLAUDE.md.
cd "${SLURM_SUBMIT_DIR:-$(dirname "$0")/../..}" || exit 1
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT" || exit 1

BIN=${BIN:-build/pin-rvs31/tactix}
N=${N:-1000}
T=${T:-10}
LONG=${LONG:-600}
MAXVARS=${MAXVARS:-60}
SEED=${SEED:-20260806}
ESCALATE=${ESCALATE:-24} # cases re-run at $LONG seconds, per leg
JOBS=${SLURM_CPUS_PER_TASK:-8}
OUT=benchmarks/probes/scratch/rv-s31
ORACLE=tools/solvers/bin/kissat

STAGE=${STAGE:-all} # all | legs | escalate (re-run one half without the other)
[ "$STAGE" = escalate ] || rm -rf "$OUT"
mkdir -p "$OUT"
echo "== rv-s31: bin=$BIN n=$N timeout=${T}s long=${LONG}s jobs=$JOBS =="
"$BIN" --version

leg() { # leg <name> <solver cmd>
    local name="$1"; shift
    echo "== leg $name: $* =="
    python3 tools/fuzz/fuzz.py run --solver "$*" --oracle "$ORACLE" \
        --n "$N" --seed "$SEED" --timeout "$T" --max-vars "$MAXVARS" \
        --jobs "$JOBS" --no-shrink --max-nonterminating-saved 400 \
        --outdir "$OUT/$name"
    echo "leg $name exit=$?"
}

if [ "$STAGE" != escalate ]; then
    leg rp-drat  "$BIN --random-policy=7"
    leg rp-lrat  "$BIN --random-policy=7 --lrat"
    leg def-drat "$BIN"
fi

# --- escalation: does a non-terminating case EVER terminate? ----------------
# check-one is driven directly (not through the bundle's repro.sh) so the whole
# command is built from absolute paths here.
if [ "$STAGE" != legs ]; then
    for name in rp-drat rp-lrat def-drat; do
        d="$OUT/$name/nonterminating"
        [ -d "$d" ] || { echo "escalate $name: none"; continue; }
        case "$name" in
            rp-lrat) sv="$ROOT/$BIN --random-policy=7 --lrat" ;;
            rp-drat) sv="$ROOT/$BIN --random-policy=7" ;;
            *)       sv="$ROOT/$BIN" ;;
        esac
        n=$(find "$d" -name case.cnf | wc -l)
        echo "== escalate $name: $n non-terminating cases, re-running up to $ESCALATE at ${LONG}s =="
        find "$d" -name case.cnf | sort | head -n "$ESCALATE" \
            | xargs -P "$JOBS" -I{} bash -c \
              'v=$(python3 '"$ROOT"'/tools/fuzz/fuzz.py check-one --solver "'"$sv"'" --timeout '"$LONG"' "{}" 2>&1 | tr "\n" " "); echo "ESCALATED {} -> $v"'
    done
fi
echo "== rv-s31 done =="
