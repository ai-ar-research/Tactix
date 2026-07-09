#!/usr/bin/env bash
# Build the proof-checker toolchain (plan §8.2 oracles; §6 compliance):
#   drat-trim   — DRAT checker + LRAT producer (-L)
#   lrat-check  — fast C LRAT checker (ships in the drat-trim repo)
#   cake_lpr    — formally verified LRAT/LPR checker (pre-verified CakeML asm)
# Clones live at the repo root, untracked (see .gitignore); binaries are
# symlinked into tools/checkers/bin/.
#
# Pinned commits — bump deliberately, never implicitly:
#   drat-trim  2e3b2dc0ecf938addbd779d42877b6ed69d9a985
#   cake_lpr   a4323b203cc9ecd584ba7da9e3fff08135a09d5f
#
# Usage: tools/checkers/setup.sh

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$ROOT/tools/checkers/bin"
mkdir -p "$BIN"

DRAT_TRIM_COMMIT=2e3b2dc0ecf938addbd779d42877b6ed69d9a985
CAKE_LPR_COMMIT=a4323b203cc9ecd584ba7da9e3fff08135a09d5f

clone_at_commit () { # dir url commit
    local dir="$ROOT/$1" url="$2" commit="$3"
    if [ ! -d "$dir" ]; then
        git clone "$url" "$dir"
        git -C "$dir" checkout --detach "$commit"
    fi
}

echo "== drat-trim + lrat-check =="
clone_at_commit drat-trim https://github.com/marijnheule/drat-trim.git "$DRAT_TRIM_COMMIT"
if [ ! -x "$ROOT/drat-trim/drat-trim" ] || [ ! -x "$ROOT/drat-trim/lrat-check" ]; then
    (cd "$ROOT/drat-trim" && make)
fi
ln -sf "$ROOT/drat-trim/drat-trim"  "$BIN/drat-trim"
ln -sf "$ROOT/drat-trim/lrat-check" "$BIN/lrat-check"

echo "== cake_lpr =="
clone_at_commit cake_lpr https://github.com/tanyongkiam/cake_lpr.git "$CAKE_LPR_COMMIT"
if [ ! -x "$ROOT/cake_lpr/cake_lpr" ]; then
    case "$(uname -m)" in
        arm64|aarch64) (cd "$ROOT/cake_lpr" && make cake_lpr_arm8) ;;
        x86_64)        (cd "$ROOT/cake_lpr" && make cake_lpr) ;;
        *) echo "unsupported arch for cake_lpr: $(uname -m)" >&2; exit 1 ;;
    esac
fi
ln -sf "$ROOT/cake_lpr/cake_lpr" "$BIN/cake_lpr"

echo "== self-test: kissat -> DRAT -> drat-trim -> LRAT -> lrat-check + cake_lpr =="
KISSAT="$ROOT/tools/solvers/bin/kissat"
if [ -x "$KISSAT" ]; then
    T="$(mktemp -d)"
    trap 'rm -rf "$T"' EXIT
    printf 'p cnf 3 6\n1 2 0\n-1 2 0\n1 -2 0\n-1 -2 3 0\n-3 2 0\n-2 -3 0\n' > "$T/f.cnf"
    "$KISSAT" -q "$T/f.cnf" "$T/f.drat" > /dev/null || [ $? -eq 20 ]
    # note: drat-trim prefixes its result line with \r, so no ^ anchor
    "$BIN/drat-trim" "$T/f.cnf" "$T/f.drat" -L "$T/f.lrat" > "$T/dt.out"
    grep -q 's VERIFIED' "$T/dt.out"
    "$BIN/lrat-check" "$T/f.cnf" "$T/f.lrat" > "$T/lc.out"
    grep -q 'VERIFIED' "$T/lc.out"
    "$BIN/cake_lpr" "$T/f.cnf" "$T/f.lrat" > "$T/cl.out"
    grep -q 's VERIFIED UNSAT' "$T/cl.out"
    echo "self-test OK"
else
    echo "self-test skipped (run tools/solvers/setup.sh first)"
fi
