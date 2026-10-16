#!/usr/bin/env bash
# Build the baseline solvers used as fuzzing oracles and benchmark baselines
# (plan §8.2, §8.3). Clones are kept at the repo root, untracked (see
# .gitignore); this script is the only supported way to (re)create them.
#
# Pinned versions — bump deliberately, never implicitly:
#   kissat  rel-4.0.4   (reference solver, plan header)
#   cadical rel-3.0.0   (second differential oracle; latest release)
#   minisat master 37dc6c6 (T1 baseline; needs two macOS/clang patches)
#   glucose 4.2.1       (T2 baseline)
#
# Usage: tools/solvers/setup.sh
# Result: binaries symlinked into tools/solvers/bin/.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$ROOT/tools/solvers/bin"
mkdir -p "$BIN"

KISSAT_TAG=rel-4.0.4
CADICAL_TAG=rel-3.0.0
MINISAT_COMMIT=37dc6c67e2af26379d88ce349eb9c4c6160e8543
GLUCOSE_TAG=4.2.1

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

echo "== minisat $MINISAT_COMMIT (T1 baseline) =="
if [ ! -d "$ROOT/minisat" ]; then
    git clone https://github.com/niklasso/minisat.git "$ROOT/minisat"
    git -C "$ROOT/minisat" checkout --detach "$MINISAT_COMMIT"
fi
if [ ! -x "$ROOT/minisat/build/release/bin/minisat" ]; then
    (cd "$ROOT/minisat" &&
     # patch 1: friend declaration may not carry the default argument
     sed -i.bak 's/friend Lit mkLit(Var var, bool sign = false);/friend Lit mkLit(Var var, bool sign);/' minisat/core/SolverTypes.h &&
     sed -i.bak 's/inline  Lit  mkLit     (Var var, bool sign) {/inline  Lit  mkLit     (Var var, bool sign = false) {/' minisat/core/SolverTypes.h &&
     # patch 2: memUsedPeak overloads must match the (bool) declaration
     sed -i.bak 's/double Minisat::memUsedPeak() {/double Minisat::memUsedPeak(bool) {/' minisat/utils/System.cc &&
     # patch 3: macOS ld has no --static
     sed -i.bak 's/MINISAT_LDFLAGS += --static \$(MINISAT_RELSYM)/MINISAT_LDFLAGS += $(MINISAT_RELSYM)/' Makefile &&
     make r -j"$(getconf _NPROCESSORS_ONLN)")
fi
ln -sf "$ROOT/minisat/build/release/bin/minisat" "$BIN/minisat"

echo "== glucose $GLUCOSE_TAG (T2 baseline) =="
clone_at_tag glucose https://github.com/audemard/glucose.git "$GLUCOSE_TAG"
if [ ! -x "$ROOT/glucose/build/glucose-simp" ]; then
    (cd "$ROOT/glucose" && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 &&
     cmake --build build -j"$(getconf _NPROCESSORS_ONLN)" --target glucose-simp)
fi
ln -sf "$ROOT/glucose/build/glucose-simp" "$BIN/glucose"

echo "== versions =="
echo "kissat  $("$BIN/kissat" --version)"
echo "cadical $("$BIN/cadical" --version)"
echo "minisat $MINISAT_COMMIT / glucose $GLUCOSE_TAG built"
