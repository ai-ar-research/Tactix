#!/usr/bin/env bash
# Fuzzing entry point (plan §8.2; fuzz skill). Wraps fuzz.py:
#   run.sh --quick  --solver CMD     ~1k cases + regression corpus (per commit)
#   run.sh --nightly --solver CMD    ~20k cases + LRAT chain
#   run.sh [fuzz.py args...]         pass-through
# Default oracle: kissat (cadical when the solver under test IS kissat).

set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

N=1000
EXTRA=()
SOLVER=""
ORACLE=""
while [ $# -gt 0 ]; do
    case "$1" in
        --quick)   N=1000;   shift ;;
        --nightly) N=20000; EXTRA+=(--lrat); shift ;;
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
python3 "$HERE/fuzz.py" corpus --solver "$SOLVER" --oracle "$ORACLE"

echo "== fuzz: $N cases, solver=$SOLVER oracle=$ORACLE =="
python3 "$HERE/fuzz.py" run --solver "$SOLVER" --oracle "$ORACLE" --n "$N" \
    ${EXTRA[@]+"${EXTRA[@]}"}
