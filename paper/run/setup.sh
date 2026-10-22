#!/usr/bin/env bash
# Build everything the paper's experiments run: proof checkers, reference
# solvers, and Tactix at each commit the paper evaluated. Everything lands in
# paper/run/tools/; rerunning skips what is already built.
#
#   paper/run/setup.sh checkers      drat-trim, cake_lpr, dsr-trim, Trestle,
#                                    VeriPB, CakePB
#   paper/run/setup.sh references    Kissat 4.0.4, Kissat-sc2024, AE-Kissat-MAB,
#                                    Satsuma+Kissat, LymphoSAT
#   paper/run/setup.sh tactix [NAME] Tactix at every commit the paper evaluated
#                                    (BUILDS in experiments.py), or one of them
#   paper/run/setup.sh all
#
# Sources: with TACTIX_THIRD_PARTY=DIR (the artifact sets it to its
# third_party/ directory) every third-party source comes from the archives in
# DIR, so the build works offline; otherwise they are downloaded at the pinned
# commits. Trestle's checker (srcheck) is a Lean 4 program that depends on
# mathlib; DIR/srcheck, a binary built from the pinned, patched source, is
# used when present, and building it from source needs Lean's `lake` and the
# network (paper/run/build_trestle.sh).
#
# Needs a C/C++ toolchain, CMake, make, zlib, xz, python3 and git.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TOOLS="$ROOT/paper/run/tools"
BIN="$TOOLS/bin"
SRC="$TOOLS/src"
SOLVERS="$TOOLS/solvers"
JOBS="${JOBS:-$(nproc)}"
TP="${TACTIX_THIRD_PARTY:-}"
# Several third-party builds declare CMake minimums that CMake 4 rejects.
export CMAKE_POLICY_VERSION_MINIMUM=3.5
# Archives keep their owners only when that is possible.
export TAR_OPTIONS="${TAR_OPTIONS:---no-same-owner}"
mkdir -p "$BIN" "$SOLVERS" "$SRC"
# Third-party C sources predate GCC 14/15's stricter defaults (compat/gcc).
THIRD_PARTY_PATH="$ROOT/paper/run/compat:$PATH"

DRAT_TRIM=(drat-trim https://github.com/marijnheule/drat-trim 2e3b2dc0ecf938addbd779d42877b6ed69d9a985)
CAKE_LPR=(cake_lpr https://github.com/tanyongkiam/cake_lpr a4323b203cc9ecd584ba7da9e3fff08135a09d5f)
DSR_TRIM=(dsr-trim https://github.com/ccodel/dsr-trim 0b8cc0ef4d228c059994923cbc66abeb9a42ea14)
TRESTLE=(trestle https://github.com/FormalSAT/trestle 03ac9418bc23d345fe0111533c198e8903362ed6)
CAKEPB=(cakepb https://gitlab.com/MIAOresearch/software/cakepb 6edb2f3585f1c3e1b925964c4f33e133f7279d02)
KISSAT4=(kissat-rel-4.0.4 https://github.com/arminbiere/kissat rel-4.0.4)

log () { printf '== %s ==\n' "$*"; }

# unpack NAME URL REF: the source tree of a pinned commit (or tag) in $SRC/NAME
unpack () {
    local name="$1" url="$2" ref="$3" dir="$SRC/$1"
    [ -d "$dir" ] && return
    local archive="$TP/$name-$ref.tar.gz"
    [ -f "$archive" ] || archive="$TP/$name.tar.gz"
    mkdir -p "$dir.part"
    if [ -n "$TP" ] && [ -f "$archive" ]; then
        tar -xzf "$archive" -C "$dir.part" --strip-components=1
    elif [[ "$url" == *gitlab.com* ]]; then
        curl -fsSL --retry 3 "$url/-/archive/$ref/$name-$ref.tar.gz" | tar -xz -C "$dir.part" --strip-components=1
    else
        curl -fsSL --retry 3 "$url/archive/$ref.tar.gz" | tar -xz -C "$dir.part" --strip-components=1
    fi
    mv "$dir.part" "$dir"
}

checkers () {
    local PATH="$THIRD_PARTY_PATH"
    log "drat-trim"
    unpack "${DRAT_TRIM[@]}"
    [ -x "$SRC/drat-trim/drat-trim" ] || make -C "$SRC/drat-trim" drat-trim >/dev/null
    ln -sf "$SRC/drat-trim/drat-trim" "$BIN/drat-trim"

    log "cake_lpr"
    unpack "${CAKE_LPR[@]}"
    if [ ! -x "$SRC/cake_lpr/cake_lpr" ]; then
        case "$(uname -m)" in
            x86_64) make -C "$SRC/cake_lpr" cake_lpr >/dev/null ;;
            aarch64|arm64) make -C "$SRC/cake_lpr" cake_lpr_arm8 >/dev/null
                           mv "$SRC/cake_lpr/cake_lpr_arm8" "$SRC/cake_lpr/cake_lpr" ;;
        esac
    fi
    ln -sf "$SRC/cake_lpr/cake_lpr" "$BIN/cake_lpr"

    log "dsr-trim (with the repository's pinned repairs)"
    unpack "${DSR_TRIM[@]}"
    if [ ! -f "$SRC/dsr-trim/.patched" ]; then
        patch -d "$SRC/dsr-trim" -p1 --quiet \
            < "$ROOT/tools/checkers/patches/dsr-trim-0b8cc0e-forward-elaboration.patch"
        touch "$SRC/dsr-trim/.patched"
    fi
    [ -x "$SRC/dsr-trim/bin/dsr-trim" ] || make -C "$SRC/dsr-trim" >/dev/null
    ln -sf "$SRC/dsr-trim/bin/dsr-trim" "$BIN/dsr-trim"
    ln -sf "$SRC/dsr-trim/bin/compress" "$BIN/dsr-compress"

    log "Trestle srcheck"
    if [ -n "$TP" ] && [ -x "$TP/srcheck" ]; then
        install -m 755 "$TP/srcheck" "$BIN/srcheck"
    elif [ ! -x "$BIN/srcheck" ]; then
        "$ROOT/paper/run/build_trestle.sh" "$BIN/srcheck"
    fi

    log "CakePB"
    unpack "${CAKEPB[@]}"
    [ -x "$SRC/cakepb/cnf/cake_pb_cnf" ] || make -C "$SRC/cakepb/cnf" >/dev/null
    ln -sf "$SRC/cakepb/cnf/cake_pb_cnf" "$BIN/cake_pb_cnf"

    log "VeriPB 3.0.2 (the binary release shipped with LymphoSAT)"
    winners
    if [ ! -d "$SRC/veripb" ]; then
        mkdir -p "$SRC/veripb"
        tar -xzf "$ROOT"/winners/lymphosat-sc2026/green/lymphosat-verified/src/third-party/veripb-3.0.2-linux-x86_64.tar.gz \
            -C "$SRC/veripb" --strip-components=1
    fi
    ln -sf "$(find "$SRC/veripb" -name veripb -type f -perm -u+x | head -1)" "$BIN/veripb"

    log "model checker and DSR transport"
    ln -sf "$ROOT/tools/fuzz/checkmodel" "$BIN/checkmodel"
    if [ ! -x "$ROOT/tools/bench/dsr-transport" ]; then
        g++ -O2 -std=c++17 -o "$ROOT/tools/bench/dsr-transport" "$ROOT/tools/bench/dsr_transport.cpp"
        sha256sum "$ROOT/tools/bench/dsr-transport" | cut -d' ' -f1 > "$ROOT/tools/bench/dsr-transport.sha256"
    fi
}

winners () {
    TACTIX_THIRD_PARTY="$TP" "$ROOT/tools/solvers/fetch-winners.sh" >/dev/null
}

references () {
    local PATH="$THIRD_PARTY_PATH"
    winners
    log "Kissat 4.0.4"
    unpack "${KISSAT4[@]}"
    local k4="$SRC/kissat-rel-4.0.4"
    [ -x "$k4/build/kissat" ] || (cd "$k4" && ./configure >/dev/null && make -j"$JOBS" kissat >/dev/null)
    ln -sf "$k4/build/kissat" "$SOLVERS/kissat-4.0.4"

    log "Kissat-sc2024"
    local k="$ROOT/winners/kissat-sc2024"
    [ -x "$k/build/kissat" ] || (cd "$k" && ./configure --competition >/dev/null && make -j"$JOBS" kissat >/dev/null)
    ln -sf "$k/build/kissat" "$SOLVERS/kissat-sc2024"

    log "AE-Kissat-MAB"
    local ae="$ROOT/winners/AE_kissat2025_MAB/AE_kissat2025_MAB"
    [ -x "$ae/build/kissat" ] || (cd "$ae" && ./configure --competition >/dev/null && make -j"$JOBS" kissat >/dev/null)
    ln -sf "$ae/build/kissat" "$SOLVERS/ae-kissat-mab"

    log "Satsuma+Kissat"
    local s="$ROOT/winners/satsuma-iter-kissat-sc2026/anders/satsuma-iter-kissat/src"
    # The archive ships binaries built for glibc 2.38; rebuild everything once.
    if [ ! -f "$TOOLS/satsuma-kissat.built" ]; then
        (cd "$s" && ./build.sh >/dev/null) && touch "$TOOLS/satsuma-kissat.built"
    fi
    ln -sfn "$s" "$SOLVERS/satsuma-kissat"

    log "LymphoSAT (portable rebuild)"
    local l="$ROOT/winners/lymphosat-sc2026/green/lymphosat/src"
    [ -f "$TOOLS/lymphosat-build.json" ] || \
        LYMPHOSAT_BUILD_REPORT="$TOOLS/lymphosat-build.json" python3 "$ROOT/paper/run/build_lymphosat.py"
    ln -sfn "$l" "$SOLVERS/lymphosat"
}

tactix () { # [build name]
    local builds
    builds="$(python3 -c 'import sys; sys.path.insert(0, sys.argv[1])
from experiments import BUILDS
wanted = sys.argv[2:]
for name, commit in BUILDS.items():
    if not wanted or name in wanted: print(name, commit)' "$ROOT/paper/run" "$@")"
    [ -n "$builds" ] || { echo "unknown build: $*" >&2; exit 1; }
    while read -r name commit; do
        local out="$TOOLS/tactix/$name"
        [ -x "$out/tactix" ] && continue
        # Three of the paper's commits live only on the paper-commits branch.
        git -C "$ROOT" cat-file -e "$commit^{commit}" 2>/dev/null ||
            git -C "$ROOT" fetch -q origin paper-commits
        log "Tactix $name (${commit:0:9})"
        local src="$SRC/tactix-$name"
        rm -rf "$src" && mkdir -p "$src"
        git -C "$ROOT" archive "$commit" | tar -x -C "$src"
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
        [ -x "$out/tactix" ] || { echo "Tactix $name did not build" >&2; exit 1; }
        echo "$commit" > "$out/COMMIT"
        rm -rf "$src"  # keep only the binaries (the sources are in git)
    done <<< "$builds"
}

case "${1:-}" in
    checkers) checkers ;;
    references) references ;;
    tactix) shift; tactix "$@" ;;
    all) checkers; references; tactix ;;
    *) sed -n '2,21p' "$0"; exit 1 ;;
esac
