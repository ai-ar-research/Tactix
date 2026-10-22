#!/usr/bin/env bash
# Fuzzing entry point (plan §8.2; fuzz skill). Wraps fuzz.py:
#   run.sh --quick  --solver CMD     ~10k cases + regression corpus (per commit)
#   run.sh --nightly --solver CMD    ~200k cases + full proof cross-checks
#   run.sh --public-random --quick --solver \
#     'build/release/tactix-public-random --pbp --random-policy={seed}'
#                                      same cases through the public API
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
PUBLIC_RANDOM=0
NIGHTLY=0
ACCEPTANCE=()
SKIP_CORPUS=0
# The corpus replays whole entries under the leg's own solver command, so a
# schedule that starves M1 can leave one of them without a verdict. That is
# fatal by default (a replay that reaches no verdict runs no oracle); the
# allowance is pre-registered per leg and has to reach BOTH phases.
# Corpus formulas survive policy retirement. Replay each under the selected
# current leg and discard historical random/stress/controller arguments.
CORPUS_EXTRA=()
while [ $# -gt 0 ]; do
    case "$1" in
        --quick)   N=10000;   shift ;;
        --nightly) N=200000; NIGHTLY=1; shift ;;
        --n)       N="$2"; shift 2 ;;
        --public-random) PUBLIC_RANDOM=1; shift ;;
        --skip-corpus) SKIP_CORPUS=1; shift ;;
        --solver)  SOLVER="$2"; shift 2 ;;
        --oracle)  ORACLE="$2"; shift 2 ;;
        --outdir)  HAS_OUTDIR=1; EXTRA+=("$1" "$2"); shift 2 ;;
        # corpus-only: the fuzz phase meters its non-terminating population
        # with --max-nonterminating-frac instead, and rejects this flag
        --allow-nonterminating)
            CORPUS_EXTRA+=("$1" "$2"); shift 2 ;;
        *) EXTRA+=("$1"); shift ;;
    esac
done
[ -n "$SOLVER" ] || { echo "usage: run.sh [--quick|--nightly] --solver CMD [fuzz.py args]" >&2; exit 2; }
[ "$HAS_OUTDIR" -eq 1 ] || {
    RUN_ID="${TACTIX_FUZZ_RUN_ID:-local-$$}"
    if [ -n "${TACTIX_FUZZ_LEG_ID:-}" ]; then
        LEG_ID="$TACTIX_FUZZ_LEG_ID"
    elif [ "$PUBLIC_RANDOM" -eq 1 ]; then
        LEG_ID=public-random
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

if [ "$PUBLIC_RANDOM" -eq 1 ]; then
    case "$SOLVER" in
        *tactix-public-random*'--random-policy={seed}'*) ;;
        *)
            echo "error: --public-random requires tactix-public-random and --random-policy={seed}" >&2
            exit 2
            ;;
    esac
fi

# This adds the LRAT cross-check only when the selected solver arm emits DRAT;
# Tactix's default PBP arm continues through VeriPB and CakePB.
if [ "$NIGHTLY" -eq 1 ]; then
    EXTRA+=(--lrat)
    # Defaults precede explicit EXTRA arguments so deliberate overrides win.
    # These are campaign-wide requirements, including for sharded runs.
    ACCEPTANCE=(--minimum-verdicts 100000 --max-nonterminating-frac 0.05)
fi

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
    ${ACCEPTANCE[@]+"${ACCEPTANCE[@]}"} \
    ${EXTRA[@]+"${EXTRA[@]}"} || RUN_STATUS=$?
echo "== corpus status $CORPUS_STATUS, fuzz status $RUN_STATUS =="
[ "$CORPUS_STATUS" -eq 0 ] && [ "$RUN_STATUS" -eq 0 ]
