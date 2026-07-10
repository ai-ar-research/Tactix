#!/usr/bin/env bash
# Deliberately broken solver for harness validation (plan §8.2): wraps kissat
# but negates every literal of the model on SAT. The harness must flag this
# via the model oracle.
set -u
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT="$("$ROOT/tools/solvers/bin/kissat" -q "$@")"
CODE=$?
if [ "$CODE" -eq 10 ]; then
    echo "$OUT" | awk '{
        if ($1 == "v") { printf "v"; for (i=2; i<=NF; i++) printf " %d", -$i; printf "\n" }
        else print
    }'
else
    echo "$OUT"
fi
exit "$CODE"
