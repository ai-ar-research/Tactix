#!/usr/bin/env bash
# Build the proof-checker toolchain (plan §8.2 oracles; §6 compliance):
#   drat-trim   — DRAT checker + LRAT producer (-L)
#   lrat-check  — fast C LRAT checker (ships in the drat-trim repo)
#   cake_lpr    — formally verified LRAT/LPR checker (pre-verified CakeML asm)
#   dsr-trim    — DSR checker + LSR elaborator
#   srcheck     — Trestle's verified textual-LSR checker
# Clones live at the repo root, untracked (see .gitignore); binaries are
# symlinked into tools/checkers/bin/.
#
# Pinned commits — bump deliberately, never implicitly:
#   drat-trim  2e3b2dc0ecf938addbd779d42877b6ed69d9a985
#   cake_lpr   a4323b203cc9ecd584ba7da9e3fff08135a09d5f
#   dsr-trim   ffe486a46a7a7894607e92fb962424c17779171c plus the
#              repository-pinned deleted-tautology hash initialization fix
#   Trestle    03ac9418bc23d345fe0111533c198e8903362ed6
#
# Usage: tools/checkers/setup.sh

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$ROOT/tools/checkers/bin"
mkdir -p "$BIN"

DRAT_TRIM_COMMIT=2e3b2dc0ecf938addbd779d42877b6ed69d9a985
CAKE_LPR_COMMIT=a4323b203cc9ecd584ba7da9e3fff08135a09d5f
DSR_TRIM_COMMIT=ffe486a46a7a7894607e92fb962424c17779171c
TRESTLE_COMMIT=03ac9418bc23d345fe0111533c198e8903362ed6
TRESTLE_FALLBACK_SHA256=f0eef5c48e2378c6fb5dfee4c7dc68ad0ff96d359067483fbf484d266f3aeb18
DSR_TRIM_PATCH="$ROOT/tools/checkers/patches/dsr-trim-ffe486-tautology-hash.patch"

clone_at_commit () { # dir url commit
    local dir="$ROOT/$1" url="$2" commit="$3"
    if [ ! -d "$dir" ]; then
        git clone "$url" "$dir"
        git -C "$dir" checkout --detach "$commit"
    fi
    local actual
    actual="$(git -C "$dir" rev-parse HEAD)"
    if [ "$actual" != "$commit" ]; then
        echo "$dir is at $actual, expected pinned commit $commit" >&2
        exit 1
    fi
    if ! git -C "$dir" diff --quiet "$commit" --; then
        echo "$dir has tracked changes relative to pinned commit $commit" >&2
        exit 1
    fi
}

clone_patched_at_commit () { # dir url commit patch
    local dir="$ROOT/$1" url="$2" commit="$3" patch="$4"
    if [ ! -d "$dir" ]; then
        git clone "$url" "$dir"
        git -C "$dir" checkout --detach "$commit"
    fi
    local actual
    actual="$(git -C "$dir" rev-parse HEAD)"
    if [ "$actual" != "$commit" ]; then
        echo "$dir is at $actual, expected pinned commit $commit" >&2
        exit 1
    fi
    if git -C "$dir" diff --quiet "$commit" --; then
        git -C "$dir" apply "$patch"
    fi
    if ! git -C "$dir" diff --binary --unified=0 "$commit" -- | cmp -s - "$patch"; then
        echo "$dir does not carry exactly the pinned patch $patch" >&2
        exit 1
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

echo "== dsr-trim + DSR compressor =="
clone_patched_at_commit dsr-trim-sr https://github.com/ccodel/dsr-trim.git \
    "$DSR_TRIM_COMMIT" "$DSR_TRIM_PATCH"
(cd "$ROOT/dsr-trim-sr" && make)
ln -sf "$ROOT/dsr-trim-sr/bin/dsr-trim" "$BIN/dsr-trim"
ln -sf "$ROOT/dsr-trim-sr/bin/compress" "$BIN/dsr-compress"

echo "== Trestle srcheck =="
clone_at_commit trestle https://github.com/FormalSAT/trestle.git \
    "$TRESTLE_COMMIT"
SR_CHECK="$ROOT/trestle/.lake/build/bin/srcheck"
if [ ! -x "$SR_CHECK" ]; then
    if command -v lake >/dev/null 2>&1; then
        (cd "$ROOT/trestle" && lake exe cache get && lake build srcheck)
    elif [ -x "$ROOT/benchmarks/baselines/sc24-26-32g-5000s/pins/srcheck" ]; then
        # Retained campaigns pin this exact Trestle build. This path keeps an
        # existing research workspace usable when Lean is not installed.
        SR_CHECK="$ROOT/benchmarks/baselines/sc24-26-32g-5000s/pins/srcheck"
        ACTUAL_SR_CHECK_SHA256="$(sha256sum "$SR_CHECK" | awk '{print $1}')"
        if [ "$ACTUAL_SR_CHECK_SHA256" != "$TRESTLE_FALLBACK_SHA256" ]; then
            echo "retained srcheck has unexpected SHA-256 $ACTUAL_SR_CHECK_SHA256" >&2
            exit 1
        fi
    else
        echo "lake is required to build the pinned Trestle srcheck" >&2
        exit 1
    fi
fi
ln -sf "$SR_CHECK" "$BIN/srcheck"

echo "== self-test: kissat -> DRAT -> drat-trim -> LRAT -> lrat-check + cake_lpr =="
KISSAT="$ROOT/tools/solvers/bin/kissat"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
if [ -x "$KISSAT" ]; then
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

echo "== self-test: DSR -> dsr-trim -> LSR -> Trestle =="
cat > "$T/sr.cnf" <<'EOF'
p cnf 6 8
1 2 0
3 4 0
-1 -3 0
-2 -4 0
5 6 0
5 -6 0
-5 6 0
-5 -6 0
EOF
cat > "$T/sr.dsr" <<'EOF'
-1 -1 3 -1 2 4 4 2 0
5 0
0
EOF
"$BIN/dsr-trim" -q -f -c "$T/sr.cnf" "$T/sr.dsr" "$T/sr.binary.lsr" \
    > "$T/dsr.out"
grep -Eq '^s VERIFIED[[:space:]]+UNSAT$' "$T/dsr.out"
test -s "$T/sr.binary.lsr"
"$BIN/dsr-compress" -d "$T/sr.binary.lsr" "$T/sr.lsr"
test -s "$T/sr.lsr"
"$BIN/srcheck" "$T/sr.cnf" "$T/sr.lsr" > "$T/srcheck.out"
grep -Eq '^s VERIFIED[[:space:]]+UNSAT$' "$T/srcheck.out"

# Pinned ffe486 deletes tautological CNF clauses while parsing, then used to
# insert those deleted slots into its duplicate hash and fail by deleting one
# twice. The repository patch skips already-deleted slots; keep this raw-CNF
# regression in both elaboration and final Trestle verification.
cat > "$T/taut.cnf" <<'EOF'
p cnf 2 3
1 -1 0
2 0
-2 0
EOF
: > "$T/taut.dsr"
"$BIN/dsr-trim" -q -c "$T/taut.cnf" "$T/taut.dsr" \
    "$T/taut.binary.lsr" > "$T/taut-dsr.out"
grep -Eq '^s VERIFIED[[:space:]]+UNSAT$' "$T/taut-dsr.out"
"$BIN/dsr-compress" -d "$T/taut.binary.lsr" "$T/taut.lsr"
"$BIN/srcheck" "$T/taut.cnf" "$T/taut.lsr" > "$T/taut-srcheck.out"
grep -Eq '^s VERIFIED[[:space:]]+UNSAT$' "$T/taut-srcheck.out"

cat > "$T/sr-bad.dsr" <<'EOF'
-1 -1 3 -1 2 4 0
5 0
0
EOF
if "$BIN/dsr-trim" -q -f -c "$T/sr.cnf" "$T/sr-bad.dsr" \
        "$T/sr-bad.lsr" > "$T/dsr-bad.out" 2>&1; then
    echo "dsr-trim accepted the invalid substitution negative control" >&2
    exit 1
fi
echo "DSR self-test OK"
