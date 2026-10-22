#!/usr/bin/env bash
# Build Trestle's verified LSR checker (srcheck) from source: the pinned
# commit plus the repository's binary-LSR patch. Trestle is a Lean 4 program
# that depends on mathlib, so this needs the network (Lean toolchain, mathlib
# and its prebuilt cache, a few GB) and takes 10-30 minutes; the artifact
# ships the resulting binary as third_party/srcheck instead.
#
#   paper/run/build_trestle.sh OUTPUT     e.g. third_party/srcheck
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$(realpath -m "${1:?usage: build_trestle.sh OUTPUT}")"
COMMIT=03ac9418bc23d345fe0111533c198e8903362ed6
WORK="${TRESTLE_WORK:-$ROOT/paper/run/tools/src/trestle-lean}"
if ! command -v lake >/dev/null 2>&1; then
    [ -x "$HOME/.elan/bin/lake" ] || curl -sSfL https://raw.githubusercontent.com/leanprover/elan/v4.1.2/elan-init.sh \
        | sh -s -- -y --default-toolchain none --no-modify-path
    export PATH="$HOME/.elan/bin:$PATH"
fi
if [ ! -d "$WORK" ]; then
    mkdir -p "$WORK.part"
    tar_src="${TACTIX_THIRD_PARTY:-}/trestle-$COMMIT.tar.gz"
    if [ -f "$tar_src" ]; then tar -xzf "$tar_src" -C "$WORK.part" --strip-components=1
    else curl -fsSL "https://github.com/FormalSAT/trestle/archive/$COMMIT.tar.gz" | tar -xz -C "$WORK.part" --strip-components=1
    fi
    patch -d "$WORK.part" -p1 --quiet < "$ROOT/tools/checkers/patches/trestle-03ac9418-binary-lsr.patch"
    mv "$WORK.part" "$WORK"
fi
cd "$WORK"
lake exe cache get        # mathlib's prebuilt .olean files for the pinned version
lake build srcheck
install -m 755 .lake/build/bin/srcheck "$OUT"
echo "srcheck -> $OUT"
