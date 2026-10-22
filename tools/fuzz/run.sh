#!/usr/bin/env bash
# Fuzzing entry point (plan §8.2; fuzz skill). Wraps fuzz.py:
#   run.sh --quick  --solver CMD     ~10k cases + regression corpus (per commit)
#   run.sh --nightly --solver CMD    ~200k cases + full proof cross-checks
#   run.sh --controller-v5 --quick --solver CMD
#                                      10k typed GF3/DirectCSP mutations
#   run.sh --forced-ttf --quick --solver CMD
#                                      10k balanced fixed-row mutations
#   run.sh [fuzz.py args...]         pass-through
# Sizes are the cluster scale (10x the workstation defaults); submit
# via tools/fuzz/slurm.sh (50 physical cores/workers), never in the session shell.
# Default oracle: kissat (cadical when the solver under test IS kissat).

set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TACTIX_REPO_ROOT:-$(cd "$HERE/../.." && pwd)}"
OUTPUT_BASE="${TACTIX_FUZZ_OUTPUT_ROOT:-$HERE/failures}"

N=10000
EXTRA=()
SOLVER=""
ORACLE=""
HAS_OUTDIR=0
CONTROLLER_V5=0
FORCED_TTF=0
NIGHTLY=0
SKIP_CORPUS=0
SKIP_FIXED_CONTROLS=0
# The corpus replays whole entries under the leg's own solver command, so a
# schedule that starves M1 can leave one of them without a verdict. That is
# fatal by default (a replay that reaches no verdict runs no oracle); the
# allowance is pre-registered per leg and has to reach BOTH phases.
CORPUS_EXTRA=()
while [ $# -gt 0 ]; do
    case "$1" in
        --quick)   N=10000;   shift ;;
        --nightly) N=200000; NIGHTLY=1; shift ;;
        --n)       N="$2"; shift 2 ;;
        --controller-v5) CONTROLLER_V5=1; shift ;;
        --forced-ttf) FORCED_TTF=1; shift ;;
        --skip-corpus) SKIP_CORPUS=1; shift ;;
        --skip-fixed-controls) SKIP_FIXED_CONTROLS=1; shift ;;
        --solver)  SOLVER="$2"; shift 2 ;;
        --oracle)  ORACLE="$2"; shift 2 ;;
        --outdir)  HAS_OUTDIR=1; EXTRA+=("$1" "$2"); shift 2 ;;
        # corpus-only: the fuzz phase meters its non-terminating population
        # with --max-nonterminating-frac instead, and rejects this flag
        --allow-nonterminating)
            CORPUS_EXTRA+=("$1" "$2"); shift 2 ;;
        # corpus-only: the leg's policy replaces entries' legacy schedules
        --replace-entry-policy)
            CORPUS_EXTRA+=("$1"); shift ;;
        *) EXTRA+=("$1"); shift ;;
    esac
done
[ -n "$SOLVER" ] || { echo "usage: run.sh [--quick|--nightly] --solver CMD [fuzz.py args]" >&2; exit 2; }
[ "$HAS_OUTDIR" -eq 1 ] || {
    RUN_ID="${TACTIX_FUZZ_RUN_ID:-local-$$}"
    if [ -n "${TACTIX_FUZZ_LEG_ID:-}" ]; then
        LEG_ID="$TACTIX_FUZZ_LEG_ID"
    elif [ "$CONTROLLER_V5" -eq 1 ]; then
        LEG_ID=controller-v5
    elif [ "$FORCED_TTF" -eq 1 ]; then
        LEG_ID=forced-ttf
    else
        LEG_ID=seeded
    fi
    DEFAULT_OUTDIR=$(python3 "$HERE/artifact_layout.py" \
        "$OUTPUT_BASE" "$RUN_ID" "$LEG_ID")
    EXTRA+=(--outdir "$DEFAULT_OUTDIR")
    echo "== artifact output: $DEFAULT_OUTDIR =="
}

if [ -z "$ORACLE" ]; then
    ORACLE="$ROOT/tools/solvers/bin/kissat"
    case "$SOLVER" in *kissat*) ORACLE="$ROOT/tools/solvers/bin/cadical" ;; esac
fi

if [ "$CONTROLLER_V5" -eq 1 ]; then
    [ "$SKIP_FIXED_CONTROLS" -eq 0 ] || EXTRA+=(--skip-trace-controls)
    echo "== controller v5 typed fuzz: $N cases, solver=$SOLVER oracle=$ORACLE =="
    exec python3 "$HERE/controller_v5.py" --solver "$SOLVER" --oracle "$ORACLE" \
        --n "$N" ${EXTRA[@]+"${EXTRA[@]}"}
fi

if [ "$FORCED_TTF" -eq 1 ]; then
    [ "$SKIP_FIXED_CONTROLS" -eq 0 ] || EXTRA+=(--skip-controls)
    echo "== forced TTF targeted fuzz: $N cases, solver=$SOLVER oracle=$ORACLE =="
    exec python3 "$HERE/forced_ttf.py" --solver "$SOLVER" --oracle "$ORACLE" \
        --n "$N" ${EXTRA[@]+"${EXTRA[@]}"}
fi

# This adds the LRAT cross-check only when the selected solver arm emits DRAT;
# Tactix's default PBP arm continues through VeriPB and CakePB.
[ "$NIGHTLY" -eq 0 ] || EXTRA+=(--lrat)

CORPUS_STATUS=0
if [ "$SKIP_CORPUS" -eq 0 ]; then
    echo "== regression corpus =="
    # The corpus gets a REAL budget, not the 5 s case default: its entries are
    # shrunk-but-not-tiny (the largest is ~900 clauses) and a sanitized build is
    # slow, and since a replay that does not reach a verdict re-runs no oracle,
    # fuzz.py counts that as a failure rather than a pass. Raise this, or pass
    # --allow-nonterminating with a reason, before weakening either side.
    # A corpus failure must not hide the seeded phase: run both, then fail.
    python3 "$HERE/fuzz.py" corpus --solver "$SOLVER" --oracle "$ORACLE" \
        --timeout 60 ${CORPUS_EXTRA[@]+"${CORPUS_EXTRA[@]}"} || CORPUS_STATUS=$?
else
    echo "== regression corpus skipped by sharded runner =="
fi

echo "== fuzz: $N cases, solver=$SOLVER oracle=$ORACLE =="
RUN_STATUS=0
python3 "$HERE/fuzz.py" run --solver "$SOLVER" --oracle "$ORACLE" --n "$N" \
    ${EXTRA[@]+"${EXTRA[@]}"} || RUN_STATUS=$?
echo "== corpus status $CORPUS_STATUS, fuzz status $RUN_STATUS =="
[ "$CORPUS_STATUS" -eq 0 ] && [ "$RUN_STATUS" -eq 0 ]
