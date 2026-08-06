#!/usr/bin/env bash
# SCHED-E1 gate, two questions in one window:
#  (a) MIGRATION EXACTNESS — moving SCHED-B3's TransitiveReduce cursor out of
#      the rule (Kernel::transitive_cursor, read inside instances()) into the
#      policy (SlotEntry::cursor) must be byte-identical. The arm that shows it
#      is `cursoff`: the two NEW cursors switched off, leaving only the migrated
#      one, which must reproduce the pre-E1 build bit for bit.
#  (b) REACHABILITY of the generalization — the `c cand-order` line must show
#      `rotated > 0` for the entries whose cursor is on, or the cursor is inert
#      here and an on/off arm would prove nothing.
# Manifest: <arm>|<stem>|<tier>|<build>|<cap>|<env...>
set -u
R=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=${TACTIX_CACHE:-/home/hwu/SAT-solving/benchmarks/cache}
IFS='|' read -r arm stem tier build cap envs <<<"$1"
BIN=$R/build/$build/tactix
f=$(ls "$CACHE"/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
[ -n "$f" ] || { echo "MISSING $stem"; exit 1; }
w=${TMPDIR:-/tmp}/e1cur-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}; mkdir -p "$w"
xz -dc "$f" >"$w/i.cnf"
# knobs IN THE COMMAND, never exported into the environment
env TACTIX_SLOT_STATS=1 $envs timeout "$cap" "$BIN" "$w/i.cnf" "$w/i.drat" \
    > "$w/out" 2>&1
echo "=== arm=$arm stem=$stem build=$build env='$envs'"
grep -E "^s |^c conflicts|^c decisions|^c propagations|^c inprocess|^c cand-order" "$w/out"
echo "proof-sha $(sha256sum "$w/i.drat" | cut -c1-32)  bytes $(stat -c %s "$w/i.drat")"
rm -rf "$w"
