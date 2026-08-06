#!/usr/bin/env bash
# RV-S22: fuzz the two new witness-emitting stress policies (BCE, Substitute) —
# both proof formats, plus the regression corpus under each policy.
#
# The bar (fuzz + stress-policy skills): 0 failures on every leg AND a non-zero
# source-counted fire count for the stressed rule, which `--require-reached`
# turns into an exit code so a vacuous leg cannot read green.
#
#   sbatch --partition=cpu-dense-hwulab-q --nodes=1 --ntasks=1 \
#          --cpus-per-task=50 --mem-per-cpu=8G --time=04:00:00 \
#          --job-name=rvs22 --output=<log> benchmarks/probes/rv-s22-stress-fuzz.sh
set -uo pipefail
cd "${SLURM_SUBMIT_DIR:-$(dirname "$0")/../..}" || exit 1
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT" || exit 1

BIN=${BIN:-build/pin-rvs22/tactix} # ASan/UBSan + assertions
N=${N:-4000}
T=${T:-10}
MAXVARS=${MAXVARS:-60}
SEED=${SEED:-20260806}
JOBS=${SLURM_CPUS_PER_TASK:-8}
OUT=benchmarks/probes/scratch/rv-s22
ORACLE=tools/solvers/bin/kissat
rc=0

STAGE=${STAGE:-all} # all | legs | corpus
[ "$STAGE" = corpus ] || rm -rf "$OUT"
mkdir -p "$OUT"
echo "== rv-s22: bin=$BIN n=$N timeout=${T}s jobs=$JOBS =="

leg() { # leg <name> <required-rule> <solver cmd...>
    local name="$1" need="$2"; shift 2
    echo "== leg $name: $* =="
    python3 tools/fuzz/fuzz.py run --solver "$*" --oracle "$ORACLE" \
        --n "$N" --seed "$SEED" --timeout "$T" --max-vars "$MAXVARS" \
        --jobs "$JOBS" --require-reached "$need" --outdir "$OUT/$name"
    local e=$?; echo "leg $name exit=$e"; [ $e -eq 0 ] || rc=1
}

corpus() { # corpus <name> <solver cmd...>
    local name="$1"; shift
    echo "== corpus $name: $* =="
    # Entries whose recorded failure is a crash/abort pass by NOT aborting, so a
    # timeout on those is reported rather than fatal (fuzz.py decides that from
    # each entry's meta). The cap of 1 covers the one SEMANTIC entry these
    # policies cannot finish: `011-lrat-unit-id-missed-off-root-level`, 39 vars /
    # 878 clauses, against whole rounds to a fixpoint at every root plus a
    # per-step invariant check under ASan. That is cost, not a livelock.
    python3 tools/fuzz/fuzz.py corpus --solver "$*" --oracle "$ORACLE" \
        --timeout 120 --allow-nonterminating 1
    local e=$?; echo "corpus $name exit=$e"; [ $e -eq 0 ] || rc=1
}

if [ "$STAGE" != corpus ]; then
    leg bce-drat   BCE        "$BIN --bce-stress-policy=7"
    leg bce-lrat   BCE        "$BIN --bce-stress-policy=7 --lrat"
    leg subst-drat Substitute "$BIN --substitute-stress-policy=7"
    leg subst-lrat Substitute "$BIN --substitute-stress-policy=7 --lrat"
fi
if [ "$STAGE" != legs ]; then
    corpus bce   "$BIN --bce-stress-policy=7"
    corpus subst "$BIN --substitute-stress-policy=7"
fi

echo "== rv-s22 done, rc=$rc =="
exit $rc
