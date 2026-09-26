#!/usr/bin/env bash
#SBATCH --job-name=am455-sweep-retarget
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --time=00:10:00
#SBATCH --cpus-per-task=2
#SBATCH --mem=4G
#SBATCH --output=benchmarks/probes/scratch/am455-sweep-fixed-target/out/%j.out
set -u
ulimit -c 0
cd /home/hwu/SAT-solving
case_dir=benchmarks/probes/scratch/am455-sweep-fixed-target
cnf=benchmarks/runs/20260925-am444-sweep-core-target-claude-fuzz/handoffs/C1-sweep-retarget-lrat/cli-min.cnf
old_dir=benchmarks/runs/20260925-am444-sweep-core-target-claude-fuzz/builds
new_dir=build/pin-am455-sweep-retarget
printf '%s  %s\n' \
  17d7fda73751a836dd090aa71c87b673c67b4bfd511238c9b4d1bd64603feb72 "$new_dir/tactix-release" \
  057c033ba05f1c35ad109e79c0662fdc09895ba3c6fc15ff138082f471b07dd2 "$new_dir/tactix-debug" | sha256sum -c - || exit 2
for mode in old-release old-debug new-release new-debug; do
  case "$mode" in
    old-release) solver="$old_dir/rel/tactix" ;;
    old-debug) solver="$old_dir/dbg/tactix" ;;
    new-release) solver="$new_dir/tactix-release" ;;
    new-debug) solver="$new_dir/tactix-debug" ;;
  esac
  "$solver" --lrat --random-policy=2475268 "$cnf" "$case_dir/out/$mode.lrat" \
    >"$case_dir/out/$mode.stdout" 2>"$case_dir/out/$mode.stderr"
  rc=$?
  printf '%s rc=%s proof_bytes=%s\n' "$mode" "$rc" \
    "$(stat -c %s "$case_dir/out/$mode.lrat" 2>/dev/null || printf absent)"
done
