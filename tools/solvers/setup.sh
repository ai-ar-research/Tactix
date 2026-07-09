#!/usr/bin/env bash
# Build the baseline solvers used as fuzzing oracles and benchmark baselines
# (plan §8.2, §8.3). Clones are kept at the repo root, untracked (see
# .gitignore); this script is the only supported way to (re)create them.
#
# Pinned versions — bump deliberately, never implicitly:
#   kissat  rel-4.0.4   (reference solver, plan header)
#   cadical rel-3.0.0   (second differential oracle; latest release)
# MiniSat and Glucose are needed by phase 1 for the T1/T2 gates; they get
# added here when that phase starts.
#
# Usage: tools/solvers/setup.sh
# Result: binaries symlinked into tools/solvers/bin/{kissat,cadical}.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$ROOT/tools/solvers/bin"
mkdir -p "$BIN"

KISSAT_TAG=rel-4.0.4
CADICAL_TAG=rel-3.0.0

clone_at_tag () { # name url tag
    local dir="$ROOT/$1" url="$2" tag="$3"
    if [ ! -d "$dir" ]; then
        git clone --depth 1 --branch "$tag" "$url" "$dir"
    fi
}

echo "== kissat $KISSAT_TAG =="
clone_at_tag kissat https://github.com/arminbiere/kissat.git "$KISSAT_TAG"
if [ ! -x "$ROOT/kissat/build/kissat" ]; then
    (cd "$ROOT/kissat" && ./configure && make -j"$(getconf _NPROCESSORS_ONLN)")
fi
ln -sf "$ROOT/kissat/build/kissat" "$BIN/kissat"

echo "== cadical $CADICAL_TAG =="
clone_at_tag cadical https://github.com/arminbiere/cadical.git "$CADICAL_TAG"
if [ ! -x "$ROOT/cadical/build/cadical" ]; then
    (cd "$ROOT/cadical" && ./configure && make -j"$(getconf _NPROCESSORS_ONLN)")
fi
ln -sf "$ROOT/cadical/build/cadical" "$BIN/cadical"

echo "== versions =="
echo "kissat  $("$BIN/kissat" --version)"
echo "cadical $("$BIN/cadical" --version)"
