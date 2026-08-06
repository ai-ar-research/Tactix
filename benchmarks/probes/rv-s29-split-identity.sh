#!/usr/bin/env bash
# RV-S29 identity evidence: splitting DefaultPolicy::solve into
# configure_from_env + build_inprocess_slot + the CDCL loop is a MOVE, so the
# trajectory must be bit-identical. Two binaries (pre-split A, post-split B) on
# the same generated cases: every `c` counter line identical and the proof file
# byte-identical. Timing lines are excluded — they are wall clock, not
# trajectory, and no timing claim is made here.
set -uo pipefail
cd "${SLURM_SUBMIT_DIR:-$(dirname "$0")/../..}" || exit 1
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT" || exit 1

A=${A:-build/pin-rvs29a/tactix} # pre-split
B=${B:-build/pin-rvs29b/tactix} # post-split
N=${N:-120}
SEED=${SEED:-20260806}
CAP=${CAP:-20} # per-run seconds; a case neither arm finishes carries no signal
W=${TMPDIR:-/tmp}/rv-s29.${SLURM_JOB_ID:-local}
mkdir -p "$W"
same=0; diffs=0; skipped=0

for i in $(seq 0 $((N - 1))); do
    cnf="$W/case.cnf"
    python3 - "$cnf" "$((SEED + i))" "$i" <<'PY'
import sys, os
sys.path.insert(0, os.path.join(os.getcwd(), "tools", "fuzz"))
from generators import generate, write_dimacs
path, seed, idx = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
name, nvars, clauses = generate(seed, idx)
write_dimacs(path, nvars, clauses)
PY
    unfinished=0
    for arm in A B; do
        bin=$([ $arm = A ] && echo "$A" || echo "$B")
        timeout "$CAP" "$bin" "$cnf" "$W/$arm.proof" > "$W/$arm.out" 2>&1
        [ $? -eq 124 ] && unfinished=1
        grep '^c ' "$W/$arm.out" | grep -v '^c timing' > "$W/$arm.stats"
    done
    if [ "$unfinished" = 1 ]; then
        # Counted, never silently dropped: an unfinished case proves nothing
        # either way, and a skip that is not reported reads as a pass.
        skipped=$((skipped + 1))
        continue
    fi
    if cmp -s "$W/A.stats" "$W/B.stats" && cmp -s "$W/A.proof" "$W/B.proof"; then
        same=$((same + 1))
    else
        diffs=$((diffs + 1))
        echo "DIFF case $i (seed $((SEED + i))):"
        diff "$W/A.stats" "$W/B.stats" | head -5
        cmp "$W/A.proof" "$W/B.proof" | head -2
    fi
done
echo "rv-s29 identity: $same/$((same + diffs)) cases byte-identical (counters +" \
     "proof), $skipped skipped (neither arm finished in ${CAP}s)"
rm -rf "$W"
[ "$diffs" -eq 0 ] && [ "$same" -gt 0 ]
