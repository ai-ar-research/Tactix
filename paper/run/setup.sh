#!/usr/bin/env bash
# Build everything the paper's experiments run: proof checkers, reference
# solvers, and Tactix at each commit the paper evaluated. Everything lands in
# paper/run/tools/; rerunning skips what is already built.
#
#   paper/run/setup.sh checkers      drat-trim, cake_lpr, dsr-trim, Trestle,
#                                    sr2drat, VeriPB, CakePB
#   paper/run/setup.sh references    Kissat 4.0.4, Kissat-sc2024, AE-Kissat-MAB,
#                                    Satsuma+Kissat, LymphoSAT
#   paper/run/setup.sh tactix [TAG]  Tactix at every paper-* tag (or one tag)
#   paper/run/setup.sh all
#
# Needs git, curl, xz, a C/C++ toolchain, CMake, zlib, and Lean's `lake`
# (for Trestle). The Docker image in paper/Dockerfile has all of them.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TOOLS="$ROOT/paper/run/tools"
BIN="$TOOLS/bin"
SOLVERS="$TOOLS/solvers"
JOBS="${JOBS:-$(nproc)}"
mkdir -p "$BIN" "$SOLVERS"

SR2DRAT_URL=https://github.com/marijnheule/sr2drat
SR2DRAT_COMMIT=ca313578e9ce23d37a3950e23a01b098aad5f310
CAKEPB_URL=https://gitlab.com/MIAOresearch/software/cakepb
CAKEPB_COMMIT=6edb2f3585f1c3e1b925964c4f33e133f7279d02

fetch_commit () { # dir url commit
    local dir="$1" url="$2" commit="$3"
    if [ ! -d "$dir/.git" ]; then
        git init -q "$dir"
        git -C "$dir" fetch -q --depth 1 "$url" "$commit"
        git -C "$dir" checkout -q --detach FETCH_HEAD
    fi
    [ "$(git -C "$dir" rev-parse HEAD)" = "$commit" ] || {
        echo "$dir is not at $commit" >&2; exit 1; }
}

checkers () {
    "$ROOT/tools/checkers/setup.sh"
    for tool in drat-trim cake_lpr dsr-trim dsr-compress srcheck; do
        ln -sf "$(readlink -f "$ROOT/tools/checkers/bin/$tool")" "$BIN/$tool"
    done
    ln -sf "$ROOT/tools/fuzz/checkmodel" "$BIN/checkmodel"

    echo "== sr2drat =="
    fetch_commit "$TOOLS/src/sr2drat" "$SR2DRAT_URL" "$SR2DRAT_COMMIT"
    make -C "$TOOLS/src/sr2drat" sr2drat >/dev/null
    ln -sf "$TOOLS/src/sr2drat/sr2drat" "$BIN/sr2drat"

    echo "== CakePB =="
    fetch_commit "$TOOLS/src/cakepb" "$CAKEPB_URL" "$CAKEPB_COMMIT"
    make -C "$TOOLS/src/cakepb/cnf" >/dev/null
    ln -sf "$TOOLS/src/cakepb/cnf/cake_pb_cnf" "$BIN/cake_pb_cnf"

    echo "== VeriPB (the binary release shipped with LymphoSAT) =="
    winners
    if [ ! -x "$TOOLS/src/veripb/veripb" ]; then
        mkdir -p "$TOOLS/src/veripb"
        tar -xzf "$ROOT"/winners/lymphosat-sc2026/green/lymphosat-verified/src/third-party/veripb-3.0.2-linux-x86_64.tar.gz \
            -C "$TOOLS/src/veripb" --strip-components=1
    fi
    ln -sf "$(find "$TOOLS/src/veripb" -name veripb -type f -perm -u+x | head -1)" "$BIN/veripb"
}

winners () {
    "$ROOT/tools/solvers/fetch-winners.sh" >/dev/null
}

references () {
    winners
    echo "== Kissat 4.0.4 =="
    "$ROOT/tools/solvers/setup.sh" >/dev/null
    ln -sf "$(readlink -f "$ROOT/tools/solvers/bin/kissat")" "$SOLVERS/kissat-4.0.4"

    echo "== Kissat-sc2024 =="
    local k="$ROOT/winners/kissat-sc2024"
    [ -x "$k/build/kissat" ] || (cd "$k" && ./configure --competition >/dev/null && make -j"$JOBS" kissat >/dev/null)
    ln -sf "$k/build/kissat" "$SOLVERS/kissat-sc2024"

    echo "== AE-Kissat-MAB =="
    local ae="$ROOT/winners/AE_kissat2025_MAB/AE_kissat2025_MAB"
    [ -x "$ae/build/kissat" ] || (cd "$ae" && ./configure --competition >/dev/null && make -j"$JOBS" kissat >/dev/null)
    ln -sf "$ae/build/kissat" "$SOLVERS/ae-kissat-mab"

    echo "== Satsuma+Kissat =="
    local s="$ROOT/winners/satsuma-iter-kissat-sc2026/anders/satsuma-iter-kissat/src"
    [ -x "$s/solver/kissat/build/kissat" ] || (cd "$s" && ./build.sh >/dev/null)
    ln -sfn "$s" "$SOLVERS/satsuma-kissat"
    ln -sf "$s/dsr-trim/bin/compress" "$BIN/satsuma-dsr-compress"

    echo "== LymphoSAT (portable rebuild) =="
    local l="$ROOT/winners/lymphosat-sc2026/green/lymphosat/src"
    [ -f "$TOOLS/lymphosat-build.json" ] || \
        LYMPHOSAT_BUILD_REPORT="$TOOLS/lymphosat-build.json" python3 "$ROOT/paper/run/build_lymphosat.py"
    ln -sfn "$l" "$SOLVERS/lymphosat"
}

tactix () { # [tag]
    local tags
    if [ $# -gt 0 ]; then tags="$1"; else tags="$(git -C "$ROOT" tag --list 'paper-*')"; fi
    [ -n "$tags" ] || { echo "no paper-* tags in this clone; run git fetch --tags" >&2; exit 1; }
    for tag in $tags; do
        local out="$TOOLS/tactix/$tag"
        [ -x "$out/tactix" ] && continue
        echo "== Tactix $tag ($(git -C "$ROOT" rev-parse --short "$tag")) =="
        local src="$TOOLS/src/tactix-$tag"
        rm -rf "$src" && mkdir -p "$src"
        git -C "$ROOT" archive "$tag" | tar -x -C "$src"
        cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-O3 >/dev/null
        mkdir -p "$out"
        # Only the solver binaries; older commits lack the later ones.
        for binary in tactix tests/tactix-public examples/tactix-cruxmiter-policy \
                      examples/tactix-chvatal-policy; do
            if cmake --build "$src/build" --parallel "$JOBS" --target "$(basename "$binary")" \
                    >/dev/null 2>&1; then
                cp "$src/build/$binary" "$out/"
            fi
        done
        [ -x "$out/tactix" ] || { echo "Tactix $tag did not build" >&2; exit 1; }
        git -C "$ROOT" rev-parse "$tag" > "$out/COMMIT"
    done
}

case "${1:-}" in
    checkers) checkers ;;
    references) references ;;
    tactix) shift; tactix "$@" ;;
    all) checkers; references; tactix ;;
    *) sed -n '2,15p' "$0"; exit 1 ;;
esac
