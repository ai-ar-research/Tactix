#!/usr/bin/env bash
#SBATCH --job-name=r4-postrepair-frontier
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --array=0-1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260827-1030-tierd-rank4-postrepair-search-frontier/logs/%A_%a.log

set -euo pipefail

campaign=/home/hwu/SAT-solving/benchmarks/runs/20260827-1030-tierd-rank4-postrepair-search-frontier
task=${SLURM_ARRAY_TASK_ID:?}
scratch=$(mktemp -d "/tmp/r4-postrepair-frontier-${SLURM_JOB_ID}-${task}.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT

case "$task" in
  0)
    arm=tactix
    prefix=TACTIX
    expected_exit=0
    binary="$campaign/pins/tactix-tactix"
    ;;
  1)
    arm=kissat
    prefix=KISSAT
    expected_exit=143
    binary="$campaign/pins/tactix-kissat"
    ;;
  *) exit 2 ;;
esac

input="$scratch/rank4.cnf"
trace="$scratch/$arm.tsv"
xz -dc /home/hwu/SAT-solving/benchmarks/cache/tierD24-26/15456f0d13b26f0b4b21525ef7d0d711-clique_n5_k10.sanitized.cnf.xz > "$input"
allowed=$(awk '/Cpus_allowed_list/ {print $2}' /proc/self/status)
first_cpu=${allowed%%[-,]*}
printf 'arm=%s node=%s allowed_cpus=%s bound_cpu=%s\n' \
  "$arm" "$(hostname)" "$allowed" "$first_cpu"

set +e
/usr/bin/time -v taskset -c "$first_cpu" env \
  "${prefix}_EVENT_TRACE=$trace" \
  "${prefix}_EVENT_TRACE_LIMIT=1000000" \
  "${prefix}_EVENT_TRACE_CONFLICT_FROM=0" \
  "${prefix}_EVENT_TRACE_CONFLICT_TO=100" \
  "${prefix}_EVENT_TRACE_STOP_AFTER_CONFLICT=100" \
  "${prefix}_EVENT_TRACE_SKIP_WATCH=1" \
  "${prefix}_EVENT_TRACE_SKIP_IMPLY=1" \
  timeout --preserve-status --signal=TERM --kill-after=5 300 \
  "$binary" "$input"
solver_exit=$?
set -e

cp "$trace" "$campaign/traces/$arm.tsv"
wc -l "$campaign/traces/$arm.tsv"
sha256sum "$campaign/traces/$arm.tsv"
printf 'arm=%s stop_after_conflict=100 solver_exit=%s expected_exit=%s\n' \
  "$arm" "$solver_exit" "$expected_exit"
[[ "$solver_exit" -eq "$expected_exit" ]]
[[ ! -e "$campaign/scratch/$task" ]]
