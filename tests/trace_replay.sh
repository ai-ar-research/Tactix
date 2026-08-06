#!/usr/bin/env bash
# --trace / --replay: the offline replay harness §8.5 requires before any reward
# proxy may be trusted (plan §5 "Reproducibility"), plus the cross-process
# determinism of --random-policy=SEED, which the whole fuzz harness rests on and
# which nothing asserted.
#
# Three things are checked, and the third is what makes the first two mean
# something:
#   REPLAY   a recorded run, replayed, reaches the same verdict AND emits a
#            byte-identical proof AND prints identical counters and model. Not
#            "the lines parsed" — the artefacts.
#   DETERMINISM  the same seed in a SECOND PROCESS produces the same trace and
#            the same proof. Same-process reproducibility would not show this.
#   NEGATIVE a trace that has been damaged must FAIL. A replay path that cannot
#            fail is worth nothing ([[ab-arm-identical-artefacts]]), so a
#            truncated trace and an unknown rule name are both required to
#            exit nonzero.
set -uo pipefail
TACTIX="$1"

T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
fails=0
note() { printf '  %s\n' "$*"; }
fail() { printf 'FAIL: %s\n' "$*"; fails=$((fails + 1)); }

# tiny UNSAT (full 2-var enumeration) and a satisfiable instance with structure
printf 'p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n' > "$T/unsat.cnf"
printf 'p cnf 6 8\n1 2 3 0\n-1 -2 0\n-1 -3 0\n-2 -3 0\n4 5 6 0\n-4 -5 0\n1 4 0\n-6 2 0\n' \
  > "$T/sat.cnf"

# stdout minus the wall-clock lines: implementation-notes §11 makes phase timers
# diagnostic-only and they move run to run on the SAME binary, so a run-identity
# comparison that includes them reports jitter as a behaviour change.
counters() { grep '^[cvs] ' "$1" | grep -v 'timing\|ms\b'; }

run_case() {
  local cnf="$1" seed="$2" fmt="$3" label="$4"
  local a="$T/a" b="$T/b"
  "$TACTIX" $fmt --random-policy="$seed" --trace="$T/t1" "$cnf" "$a.proof" \
    > "$a.out" 2>"$a.err"
  local ra=$?
  "$TACTIX" $fmt --replay="$T/t1" "$cnf" "$b.proof" > "$b.out" 2>"$b.err"
  local rb=$?
  [ "$ra" = "$rb" ] || fail "$label: verdict $ra recorded, $rb replayed"
  cmp -s "$a.proof" "$b.proof" ||
    fail "$label: replay proof differs from the recorded run's"
  counters "$a.out" > "$a.cnt"; counters "$b.out" > "$b.cnt"
  cmp -s "$a.cnt" "$b.cnt" || fail "$label: replay counters/model differ"

  # DETERMINISM: the same seed, a second process, must produce the same trace
  "$TACTIX" $fmt --random-policy="$seed" --trace="$T/t2" "$cnf" "$T/c.proof" \
    > /dev/null 2>&1
  cmp -s "$T/t1" "$T/t2" ||
    fail "$label: --random-policy=$seed is not reproducible across processes"
  cmp -s "$a.proof" "$T/c.proof" ||
    fail "$label: same seed, different proof across processes"
  note "$label: verdict $ra, $(grep -cv '^c ' "$T/t1") actions, proof + counters identical, seed reproducible"
}

run_case "$T/unsat.cnf" 7  ""        "unsat/drat"
run_case "$T/unsat.cnf" 7  "--lrat"  "unsat/lrat"
run_case "$T/unsat.cnf" 23 ""        "unsat/drat seed 23"
run_case "$T/sat.cnf"   11 ""        "sat/drat"
run_case "$T/sat.cnf"   11 "--lrat"  "sat/lrat"

# --- NEGATIVE CONTROLS -----------------------------------------------------
"$TACTIX" -q --random-policy=7 --trace="$T/neg" "$T/unsat.cnf" "$T/n.proof" \
  > /dev/null 2>&1
# (a) a truncated trace: the recorded verdict is never reached
grep -v '^c verdict' "$T/neg" | head -n -1 > "$T/trunc"
grep '^c verdict' "$T/neg" >> "$T/trunc"
if "$TACTIX" -q --replay="$T/trunc" "$T/unsat.cnf" "$T/t.proof" >/dev/null 2>&1
then fail "a truncated trace replayed as if it were complete"
else note "negative: a truncated trace fails the verdict check"
fi
# (b) an unknown rule name
sed 's/^Reorder/Nonesuch/' "$T/neg" > "$T/badname"
if ! cmp -s "$T/neg" "$T/badname"; then
  if "$TACTIX" -q --replay="$T/badname" "$T/unsat.cnf" "$T/t.proof" \
      >/dev/null 2>&1
  then fail "a trace naming an unknown rule replayed anyway"
  else note "negative: an unknown rule name is rejected"
  fi
fi
# (c) --trace on a policy that is not a (rule, instance) sequence must refuse
if "$TACTIX" -q --trace="$T/never" "$T/unsat.cnf" >/dev/null 2>&1
then fail "--trace was accepted for DefaultPolicy, which cannot be traced"
else note "negative: --trace refuses a non-traceable policy"
fi

[ "$fails" = 0 ] || { echo "trace_replay: $fails failure(s)"; exit 1; }
echo "trace_replay: all passed"
