#!/usr/bin/env bash
#SBATCH --job-name=r4-defer-h-prefix
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260827-1045-rank4-deferred-guidance-prefix/logs/%j.log

set -euo pipefail

campaign=/home/hwu/SAT-solving/benchmarks/runs/20260827-1045-rank4-deferred-guidance-prefix
scratch=$(mktemp -d "/tmp/r4-defer-h-prefix-${SLURM_JOB_ID}.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT
input="$scratch/rank4.cnf"
trace="$scratch/tactix.tsv"
xz -dc /home/hwu/SAT-solving/benchmarks/cache/tierD24-26/15456f0d13b26f0b4b21525ef7d0d711-clique_n5_k10.sanitized.cnf.xz > "$input"
allowed=$(awk '/Cpus_allowed_list/ {print $2}' /proc/self/status)
first_cpu=${allowed%%[-,]*}
printf 'node=%s allowed_cpus=%s bound_cpu=%s\n' "$(hostname)" "$allowed" "$first_cpu"

set +e
/usr/bin/time -v taskset -c "$first_cpu" env \
  TACTIX_EVENT_TRACE="$trace" \
  TACTIX_EVENT_TRACE_LIMIT=1000000 \
  TACTIX_EVENT_TRACE_CONFLICT_FROM=0 \
  TACTIX_EVENT_TRACE_CONFLICT_TO=100 \
  TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT=100 \
  TACTIX_EVENT_TRACE_SKIP_WATCH=1 \
  TACTIX_EVENT_TRACE_SKIP_IMPLY=1 \
  timeout --preserve-status --signal=TERM --kill-after=5 300 \
  "$campaign/pins/tactix-tactix" "$input"
solver_exit=$?
set -e

cp "$trace" "$campaign/traces/tactix.tsv"
wc -l "$campaign/traces/tactix.tsv"
sha256sum "$campaign/traces/tactix.tsv"
printf 'stop_after_conflict=100 solver_exit=%s expected_exit=0\n' "$solver_exit"
[[ "$solver_exit" -eq 0 ]]
