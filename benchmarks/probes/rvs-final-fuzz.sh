#!/usr/bin/env bash
# Closing soundness gate for the whole `rvs-coverage-hygiene` branch: the FINAL
# tree (DefaultPolicy::solve split included) through the four oracles on both
# proof formats, plus a random-policy leg and the regression corpus.
#
#   sbatch --partition=cpu-dense-hwulab-q --nodes=1 --ntasks=1 \
#          --cpus-per-task=50 --mem-per-cpu=8G --time=04:00:00 \
#          --job-name=rvsfinal --output=<log> benchmarks/probes/rvs-final-fuzz.sh
set -uo pipefail
cd "${SLURM_SUBMIT_DIR:-$(dirname "$0")/../..}" || exit 1
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT" || exit 1

BIN=${BIN:-build/pin-rvsfinal/tactix} # ASan/UBSan + assertions
N=${N:-3000}
T=${T:-10}
JOBS=${SLURM_CPUS_PER_TASK:-8}
OUT=benchmarks/probes/scratch/rvs-final
ORACLE=tools/solvers/bin/kissat
rc=0
rm -rf "$OUT"; mkdir -p "$OUT"

leg() { # leg <name> <solver cmd...>
    local name="$1"; shift
    echo "== leg $name: $* =="
    python3 tools/fuzz/fuzz.py run --solver "$*" --oracle "$ORACLE" --n "$N" \
        --seed 20260806 --timeout "$T" --max-vars 60 --jobs "$JOBS" \
        --outdir "$OUT/$name"
    local e=$?; echo "leg $name exit=$e"; [ $e -eq 0 ] || rc=1
}

leg def-drat "$BIN"
leg def-lrat "$BIN --lrat"
leg rp-lrat  "$BIN --random-policy=7 --lrat"
echo "== corpus =="
python3 tools/fuzz/fuzz.py corpus --solver "$BIN" --oracle "$ORACLE" --timeout 60
e=$?; echo "corpus exit=$e"; [ $e -eq 0 ] || rc=1
echo "== rvs-final done, rc=$rc =="
exit $rc
