#!/usr/bin/env bash
# Demonstrates the TACTIX_FACTOR_DENSITY uint32 overflow and its fix.
#
# `admit = binary_ids.size() >= density * kernel_num_output_vars(k)` multiplied
# two uint32s, so the bound WRAPPED.  tierC/128_125 has 48,505,464 variables, so
# any density >= 89 overflows there: an A/B arm setting density HIGH to suppress
# Factor would wrap to a small bound and ADMIT it instead -- the knob inverts
# exactly where it is used as an off-switch.  The shipped default of 6 is safe
# everywhere (6 * 48.5M = 291M), so nothing shipped was wrong.
#
# PRE-REGISTERED READING: DecompressReader has 228 vars, so density 18,837,576
# gives 18837576*228 = 4,294,967,328, which WRAPS to 32 in uint32. `pre`
# (pin-tb4b) must therefore ADMIT Factor (bound 32) where `post` (pin-facablate,
# uint64) must NOT (bound 4.29e9). At density=6 the two must agree -- that is
# the behaviour-preservation control.
# Manifest: <label>|<stem>|<tier>|<build>|<density>|<cap>
set -u
R=/home/hwu/SAT-solving
IFS='|' read -r label stem tier build density cap <<<"$1"
BIN=$R/build/$build/tactix
[ -x "$BIN" ] || { echo "MISSING BINARY $BIN"; exit 1; }
f=$(ls "$R"/benchmarks/cache/"$tier"/${stem}*.cnf.xz 2>/dev/null | head -1)
work=${TMPDIR:-/tmp}/dov-${SLURM_ARRAY_JOB_ID:-x}_${SLURM_ARRAY_TASK_ID:-$$}
mkdir -p "$work"; xz -dc "$f" >"$work/i.cnf" || exit 1
export TACTIX_FACTOR_DENSITY="$density"
echo "=== label=$label build=$build density=$density bin-sha $(sha256sum "$BIN" | cut -c1-16)"
grep -m1 '^p cnf' "$work/i.cnf"
timeout "$cap" "$BIN" "$work/i.cnf" "$work/i.drat" 2>&1 | grep -E '^s |^c conflicts|^c factor|^c extension'
rm -rf "$work"
