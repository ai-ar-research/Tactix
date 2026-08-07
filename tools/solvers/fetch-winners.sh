#!/usr/bin/env bash
# Fetch recent SAT Competition winners' SOURCES as reference material
# (plan §5 "reference solvers are evidence, not templates", rev 14; §8.3
# lists them as baselines "where obtainable"). Sources only — these are
# StarExec/Linux-targeted packages; building them for benchmark baselines
# is a separate, deliberate step (revisit at the T4/T5 gates).
#
# Everything lands in winners/ at the repo root, untracked (see .gitignore),
# same convention as the kissat/ and cadical/ reference clones.
#
#   AE-Kissat-MAB   SC 2025 main-track winner (MAB restart tuning on kissat)
#   CaDiCaL-SC2025  SC 2025 main-track UNSAT winner
#   kissat-sc2025   Biere et al.'s own SC 2025 entry (2nd overall)
#   kissat-sc2024   SC 2024 main-track winner (swept all three golds); the
#                   SC 2024 site never published solver archives ("tbd"),
#                   so this comes from the kissat repo's sc2024 tag — the
#                   pinned rel-4.0.4 clone is this winner's lineage a few
#                   releases later, so diffs against it are small and telling
#   SBVA            SC 2023 winner's preprocessor (SBVA-CaDiCaL), from the
#                   authors' GitHub (paper artifact, pinned commit)
#
# Usage: tools/solvers/fetch-winners.sh

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DIR="$ROOT/winners"
mkdir -p "$DIR"

SC25=https://satcompetition.github.io/2025/downloads/solvers/main
SBVA_REPO=https://github.com/hgarrereyn/SBVA.git
SBVA_COMMIT=35e74d02aa1b0970d53fef1c86171e596d0da453

fetch_tar () { # name url
    local name="$1" url="$2"
    if [ -d "$DIR/$name" ]; then
        echo "== $name: already present =="
        return
    fi
    echo "== $name: fetching =="
    local tmp="$DIR/.tmp.$name"
    rm -rf "$tmp"
    mkdir -p "$tmp"
    # extract into a temp dir first so an interrupted fetch never leaves a
    # partial directory that a re-run would mistake for a complete one
    curl -fsSL "$url" | tar -xJ -C "$tmp"
    echo "$url" > "$tmp/SOURCE_URL"
    mv "$tmp" "$DIR/$name"
}

fetch_tar AE_kissat2025_MAB "$SC25/AE_kissat2025_MAB.tar.xz"
fetch_tar cadical-sc2025    "$SC25/cadical-sc2025.tar.xz"
fetch_tar kissat-sc2025     "$SC25/kissat-sc2025.tar.xz"

if [ ! -d "$DIR/kissat-sc2024" ]; then
    echo "== kissat-sc2024: cloning at tag sc2024 =="
    git clone --depth 1 --branch sc2024 https://github.com/arminbiere/kissat.git \
        "$DIR/kissat-sc2024"
fi

if [ ! -d "$DIR/SBVA" ]; then
    echo "== SBVA: cloning =="
    git clone "$SBVA_REPO" "$DIR/SBVA"
    git -C "$DIR/SBVA" checkout --detach "$SBVA_COMMIT" 2>/dev/null ||
        echo "warning: pinned SBVA commit not found; using default branch HEAD"
fi

echo "done: $(ls "$DIR" | tr '\n' ' ')"
