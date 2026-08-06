#!/usr/bin/env bash
# Anti-vacuity check for the fuzz harness's NEW gates (RV-S31 / RV-S23): each
# one must be able to FAIL. A green conformance signal that cannot go red is
# worth nothing, and these three gates are what the coverage claims now rest on.
#
#   1. --require-reached on a rule the leg cannot fire  -> exit 1
#   2. --max-nonterminating-frac 0 with a policy that livelocks -> exit 1
#   3. --require-reached with an unknown rule name      -> exit 2 (never a
#      silent requirement that can neither fire nor fail)
#   4. the same leg with a rule it DOES fire            -> exit 0 (no false alarm)
set -uo pipefail
cd "${SLURM_SUBMIT_DIR:-$(dirname "$0")/../..}" || exit 1
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT" || exit 1
BIN=${BIN:-build/pin-rvs31/tactix}
OUT=benchmarks/probes/scratch/rv-s31-gates
JOBS=${SLURM_CPUS_PER_TASK:-4}
rm -rf "$OUT"; mkdir -p "$OUT"
rc=0

check() { # check <expected-exit> <label> <args...>
    local want="$1" label="$2"; shift 2
    python3 tools/fuzz/fuzz.py run "$@" > "$OUT/$label.log" 2>&1
    local got=$?
    printf '%-34s expected exit %s, got %s  %s\n' "$label" "$want" "$got" \
        "$([ "$got" = "$want" ] && echo OK || { echo MISMATCH; rc=1; })"
    tail -3 "$OUT/$label.log" | sed 's/^/    /'
}

# Factor cannot fire on 40 tiny cases under DefaultPolicy (measured: 0 of 1000)
check 1 require-reached-unfired --solver "$BIN" --n 40 --seed 424242 \
      --timeout 5 --max-vars 40 --jobs "$JOBS" --outdir "$OUT/a" \
      --require-reached Factor
# ... while Propagate fires on essentially every case
check 0 require-reached-fired --solver "$BIN" --n 40 --seed 424242 \
      --timeout 5 --max-vars 40 --jobs "$JOBS" --outdir "$OUT/b" \
      --require-reached Propagate
# the random policy loses double digits of coverage; a 0 budget must fail
check 1 nonterminating-budget --solver "$BIN --random-policy=7" --n 60 \
      --seed 20260806 --timeout 5 --max-vars 60 --jobs "$JOBS" \
      --outdir "$OUT/c" --max-nonterminating-frac 0
# a misspelled rule name is a requirement that can never fire
check 2 unknown-rule-name --solver "$BIN" --n 4 --timeout 5 --jobs 1 \
      --outdir "$OUT/d" --require-reached Factorr

echo "rv-s31 harness gates: rc=$rc"
exit $rc
