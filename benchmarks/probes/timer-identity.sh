#!/usr/bin/env bash
# Phase timers must be pure bookkeeping: the `c timing` block reads clocks, and a
# clock must never reach a guard or a decision or runs stop being reproducible.
# Static check (grep) says the fields are written in registry.cpp/default_policy
# and read only in main.cpp. This is the empirical half: same instance, timer
# build vs the pre-timer HEAD build, conflicts/decisions/propagations must match
# EXACTLY. Any difference means a clock leaked into the trajectory.
set -u
# OLD was a worktree at $HOME/tactix-pretimer, removed in the 2026-08-03 re-org.
# It is fully reproducible -- fdc0bea is the pre-timer HEAD:
#   git worktree add .claude/worktrees/pretimer fdc0bea
#   cmake -S <that> -B <that>/build/release -DCMAKE_BUILD_TYPE=Release && cmake --build ...
# then point OLD at it. Never build a worktree over a path an in-flight job uses.
ROOT=$(git rev-parse --show-toplevel)
NEW=${NEW:-$ROOT/build/release/tactix}
OLD=${OLD:?set OLD to a pre-timer build -- see the header (fdc0bea)}
inst="$1"
f=$(ls "$ROOT"/benchmarks/cache/tier[BC]/${inst}*.cnf.xz | head -1)
cnf=$TMPDIR/$(basename "$f" .cnf.xz).cnf
xz -dc "$f" > "$cnf"
for b in OLD NEW; do
  bin=$(eval echo \$$b)
  echo "=== $b $(basename "$inst") ==="
  /usr/bin/time -f "wall %e" timeout 900 "$bin" "$cnf" 2>&1 |
    grep -E "^s |^c conflicts|^c decisions|^c propagations|^c learned |wall "
done
rm -f "$cnf"
