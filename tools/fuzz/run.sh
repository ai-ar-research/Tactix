#!/usr/bin/env bash
# Fuzzing entry point (plan §8.2; fuzz skill). Wraps fuzz.py:
#   run.sh --quick  --solver CMD     ~10k cases + regression corpus (per commit)
#   run.sh --nightly --solver CMD    ~200k cases + LRAT chain
#   run.sh [fuzz.py args...]         pass-through
# Sizes are the cluster scale (10x the workstation defaults); submit
# via tools/fuzz/slurm.sh (50 threads), never in the session shell.
# Default oracle: kissat (cadical when the solver under test IS kissat).

set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

N=10000
EXTRA=()
SOLVER=""
ORACLE=""
while [ $# -gt 0 ]; do
    case "$1" in
        --quick)   N=10000;   shift ;;
        --nightly) N=200000; EXTRA+=(--lrat); shift ;;
        --solver)  SOLVER="$2"; shift 2 ;;
        --oracle)  ORACLE="$2"; shift 2 ;;
        *) EXTRA+=("$1"); shift ;;
    esac
done
[ -n "$SOLVER" ] || { echo "usage: run.sh [--quick|--nightly] --solver CMD [fuzz.py args]" >&2; exit 2; }

if [ -z "$ORACLE" ]; then
    ORACLE="$ROOT/tools/solvers/bin/kissat"
    case "$SOLVER" in *kissat*) ORACLE="$ROOT/tools/solvers/bin/cadical" ;; esac
fi

echo "== regression corpus =="
# The corpus gets a REAL budget, not the 5 s case default: its entries are
# shrunk-but-not-tiny (the largest is ~900 clauses) and a sanitized build is
# slow, and since a replay that does not reach a verdict re-runs no oracle,
# fuzz.py counts that as a failure rather than a pass. Raise this, or pass
# --allow-nonterminating with a reason, before weakening either side.
python3 "$HERE/fuzz.py" corpus --solver "$SOLVER" --oracle "$ORACLE" \
    --timeout 60

echo "== fuzz: $N cases, solver=$SOLVER oracle=$ORACLE =="
python3 "$HERE/fuzz.py" run --solver "$SOLVER" --oracle "$ORACLE" --n "$N" \
    ${EXTRA[@]+"${EXTRA[@]}"}
