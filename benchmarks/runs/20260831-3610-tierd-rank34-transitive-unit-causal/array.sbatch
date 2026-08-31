#!/usr/bin/env bash
#SBATCH --job-name=r34-transitive-unit
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --array=0-1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:08:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260831-3610-tierd-rank34-transitive-unit-causal/logs/%A_%a.log

set -euo pipefail
campaign=/home/hwu/SAT-solving/benchmarks/runs/20260831-3610-tierd-rank34-transitive-unit-causal
input_xz=/home/hwu/SAT-solving/benchmarks/cache/tierD24-26/0ae917c2e0c1d39cbe411c29b2f5ff08-dislog_a14_x14_n24.cnf.xz
expected_input_sha=ae23202ebf6de47a653fb74430a7603e155a321f2826929864e9bfb69c41ebbd
pairs='-19396,19343;-19788,19343'
if (( SLURM_ARRAY_TASK_ID == 0 )); then
  arm=tactix; prefix=TACTIX; solver=$campaign/pins/tactix-trace; expected_solver_sha=c8b7737054990c4558b5d25f89870e4bfe82ae062ca888fcab901a83df518f11
else
  arm=kissat; prefix=KISSAT; solver=$campaign/pins/kissat-trace; expected_solver_sha=b19089eb8ecdfb7709f8f13345bfd0497dcd7a6a356d2cd280454dffd60c5cb5
fi
scratch_root=${SLURM_TMPDIR:-/tmp}
scratch=$(mktemp -d "$scratch_root/r34-transitive-unit-${SLURM_JOB_ID}_${SLURM_ARRAY_TASK_ID}.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT
input=$scratch/rank34.cnf
trace=$scratch/trace.tsv
transitive_trace=$scratch/transitive.tsv
binary_set=$scratch/binary-set.tsv
[[ $(sha256sum "$input_xz" | awk '{print $1}') == "$expected_input_sha" ]]
[[ $(sha256sum "$solver" | awk '{print $1}') == "$expected_solver_sha" ]]
xz -dc "$input_xz" > "$input"
allowed=$(awk '/Cpus_allowed_list/ {print $2}' /proc/self/status)
first_cpu=${allowed%%[-,]*}
set +e
/usr/bin/time -v taskset -c "$first_cpu" env \
  "${prefix}_EVENT_TRACE=$trace" "${prefix}_EVENT_TRACE_LIMIT=200000" \
  "${prefix}_EVENT_TRACE_CONFLICT_FROM=7705" \
  "${prefix}_EVENT_TRACE_CONFLICT_TO=7706" \
  "${prefix}_EVENT_TRACE_STOP_AFTER_CONFLICT=7706" \
  "${prefix}_EVENT_TRACE_KEEP_SNAPSHOTS=1" \
  "${prefix}_EVENT_TRACE_SKIP_WATCH=1" \
  "${prefix}_EVENT_TRACE_SKIP_IMPLY=1" \
  "${prefix}_EVENT_TRACE_SKIP_HEAP=1" \
  "${prefix}_TRANSITIVE_TRACE=$transitive_trace" \
  "${prefix}_TRANSITIVE_TRACE_PAIRS=$pairs" \
  "SAT_EVENT_TRACE_BINARY_SET_PATH=$binary_set" \
  "SAT_EVENT_TRACE_BINARY_SET_MATCH=ransitive" \
  "SAT_EVENT_TRACE_BINARY_SET_CONFLICT=7705" \
  timeout --preserve-status --signal=TERM --kill-after=5 360 "$solver" "$input"
solver_exit=$?
set -e
case "$solver_exit" in 0|10|20|143) ;; *) exit "$solver_exit" ;; esac
[[ -s $trace ]]
[[ -s $transitive_trace ]]
[[ -s $binary_set ]]
cp "$trace" "$campaign/traces/$arm.tsv"
cp "$transitive_trace" "$campaign/transitive-traces/$arm.tsv"
cp "$binary_set" "$campaign/binary-sets/$arm.tsv"
wc -l "$campaign/traces/$arm.tsv" \
  "$campaign/transitive-traces/$arm.tsv" \
  "$campaign/binary-sets/$arm.tsv"
sha256sum "$campaign/traces/$arm.tsv" \
  "$campaign/transitive-traces/$arm.tsv" \
  "$campaign/binary-sets/$arm.tsv"
printf 'arm=%s pairs=%s match=ransitive conflict=7705 solver_exit=%s\n' \
  "$arm" "$pairs" "$solver_exit"
