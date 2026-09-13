#!/usr/bin/env bash
# --trace / --replay: the offline replay harness §8.5 requires before any reward
# proxy may be trusted (plan §5 "Reproducibility"), plus the cross-process
# determinism of --random-policy=SEED that the fuzz harness rests on.
#
#   replay        a recorded run, replayed, reaches the same verdict and emits a
#                 byte-identical proof, counters and model -- the artefacts, not
#                 just parseable lines.
#   determinism   the same seed in a second process produces the same trace and
#                 proof; same-process reproducibility would not show this.
#   negative      a damaged trace must fail, so a truncated trace and an unknown
#                 rule name must both exit nonzero.
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
printf 'p cnf 0 0\n' > "$T/empty.cnf"

# stdout minus the wall-clock lines: phase timers are diagnostic-only
# (docs/implementation-notes.md §11) and move run to run on one binary, so a
# run-identity comparison including them reports jitter as a behaviour change.
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
  grep -q '^p .*sequence-repeat=' "$T/t1" ||
    fail "$label: trace omitted the typed online program"
  cmp -s "$a.proof" "$b.proof" ||
    fail "$label: replay proof differs from the recorded run's"
  counters "$a.out" > "$a.cnt"; counters "$b.out" > "$b.cnt"
  cmp -s "$a.cnt" "$b.cnt" || fail "$label: replay counters/model differ"

  # the same seed in a second process must produce the same trace
  "$TACTIX" $fmt --random-policy="$seed" --trace="$T/t2" "$cnf" "$T/c.proof" \
    > /dev/null 2>&1
  cmp -s "$T/t1" "$T/t2" ||
    fail "$label: --random-policy=$seed is not reproducible across processes"
  cmp -s "$a.proof" "$T/c.proof" ||
    fail "$label: same seed, different proof across processes"
  note "$label: verdict $ra, $(grep -c '^p ' "$T/t1") typed programs, proof + counters identical, seed reproducible"
}

run_case "$T/unsat.cnf" 7  ""        "unsat/drat"
run_case "$T/unsat.cnf" 7  "--lrat"  "unsat/lrat"
run_case "$T/unsat.cnf" 23 ""        "unsat/drat seed 23"
run_case "$T/sat.cnf"   11 ""        "sat/drat"
run_case "$T/sat.cnf"   11 "--lrat"  "sat/lrat"

# Parsing alone can complete a formula before RandomPolicy generates a
# program. That zero-program trace is still a complete replayable execution.
"$TACTIX" -q --random-policy=5 --trace="$T/zero" "$T/empty.cnf" \
  > "$T/zero.out" 2>"$T/zero.err"
zero_rc=$?
"$TACTIX" -q --replay="$T/zero" "$T/empty.cnf" \
  > "$T/zero-replay.out" 2>"$T/zero-replay.err"
zero_replay_rc=$?
if [ "$zero_rc" -eq 10 ] && [ "$zero_replay_rc" -eq 10 ] && \
   ! grep -q '^p ' "$T/zero"; then
  note "sat/zero-program: parsing verdict replays"
else
  fail "a verdict reached before the first program did not replay"
fi

# --- negative controls -----------------------------------------------------
"$TACTIX" -q --random-policy=7 --trace="$T/neg" "$T/unsat.cnf" "$T/n.proof" \
  > /dev/null 2>&1
# (a) a truncated trace: the recorded verdict is never reached
grep -v '^c verdict' "$T/neg" | head -n -1 > "$T/trunc"
grep '^c verdict' "$T/neg" >> "$T/trunc"
if "$TACTIX" -q --replay="$T/trunc" "$T/unsat.cnf" "$T/t.proof" >/dev/null 2>&1
then fail "a truncated trace replayed as if it were complete"
else note "negative: a truncated trace fails the verdict check"
fi
# (b) a historical action trace still replays, but an unknown rule does not.
cat > "$T/legacy" <<'EOF'
c tactix-trace 2
c policy random seed=7
Vivify -
Rephase -
Factor v:1
Probe v:1
Rephase v:2
Sweep v:1
c verdict 20
EOF
"$TACTIX" -q --replay="$T/legacy" "$T/unsat.cnf" "$T/legacy.proof" \
  >/dev/null 2>&1
legacy_rc=$?
if [ "$legacy_rc" -eq 20 ]; then
  note "compatibility: a historical v2 action trace still replays"
else
  fail "a historical v2 action trace no longer replays (rc=$legacy_rc)"
fi
awk 'BEGIN { changed=0 }
     /^c / { print; next }
     !changed { $1="Nonesuch"; changed=1 }
     { print }' "$T/legacy" > "$T/badname"
if "$TACTIX" -q --replay="$T/badname" "$T/unsat.cnf" "$T/t.proof" \
    >/dev/null 2>&1
then fail "a trace naming an unknown rule replayed anyway"
else note "negative: an unknown rule name is rejected"
fi
# (c) corrupt a typed online program rather than one resulting action
"$TACTIX" -q --random-policy=7 --trace="$T/oneg" \
  "$T/unsat.cnf" "$T/on.proof" >/dev/null 2>&1
sed '0,/sequence-repeat=/s//sequence-replay=/' "$T/oneg" > "$T/badprogram"
if "$TACTIX" -q --replay="$T/badprogram" "$T/unsat.cnf" "$T/t.proof" \
    >/dev/null 2>&1
then fail "a malformed online program replayed anyway"
else note "negative: a malformed online program is rejected"
fi
# (d) --trace on a policy that is not a replayable schedule must refuse
if "$TACTIX" -q --trace="$T/never" "$T/unsat.cnf" >/dev/null 2>&1
then fail "--trace was accepted for DefaultPolicy, which cannot be traced"
else note "negative: --trace refuses a non-traceable policy"
fi

[ "$fails" = 0 ] || { echo "trace_replay: $fails failure(s)"; exit 1; }
echo "trace_replay: all passed"
