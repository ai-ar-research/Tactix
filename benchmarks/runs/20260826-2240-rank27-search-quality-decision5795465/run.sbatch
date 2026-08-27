#!/usr/bin/env bash
#SBATCH --job-name=r27-search-quality-5795465
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --array=0-1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --nodelist=node018
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260826-2240-rank27-search-quality-decision5795465/logs/%A_%a.log

set -euo pipefail
campaign=/home/hwu/SAT-solving/benchmarks/runs/20260826-2240-rank27-search-quality-decision5795465
task=${SLURM_ARRAY_TASK_ID:?}
scratch=$(mktemp -d "/tmp/r27-search-quality-5795465-${SLURM_JOB_ID}-${task}.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT

case "$task" in
  0)
    arm=tactix
    prefix=TACTIX
    binary="$campaign/pins/tactix-current-trace"
    expected_exit=0
    ;;
  1)
    arm=kissat
    prefix=KISSAT
    binary="$campaign/pins/kissat-current-trace"
    expected_exit=143
    ;;
  *) exit 2 ;;
esac

input="$scratch/rank27.cnf"
trace="$scratch/$arm.tsv"
xz -dc /home/hwu/SAT-solving/benchmarks/cache/tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz > "$input"
allowed=$(awk '/Cpus_allowed_list/ {print $2}' /proc/self/status)
first_cpu=${allowed%%[-,]*}
echo "arm=$arm node=$(hostname) allowed_cpus=$allowed bound_cpu=$first_cpu expected_exit=$expected_exit"
findmnt -T "$scratch" -n -o SOURCE,FSTYPE,TARGET
ulimit -v 16777216

set +e
/usr/bin/time -v taskset -c "$first_cpu" env \
  "${prefix}_EVENT_TRACE=$trace" \
  "${prefix}_EVENT_TRACE_LIMIT=150000" \
  "${prefix}_EVENT_TRACE_CONTROL_ONLY=1" \
  "${prefix}_EVENT_TRACE_SKIP_WATCH=1" \
  "${prefix}_EVENT_TRACE_SKIP_IMPLY=1" \
  "${prefix}_EVENT_TRACE_CONFLICT_FROM=9459" \
  "${prefix}_EVENT_TRACE_CONFLICT_TO=9459" \
  "${prefix}_EVENT_TRACE_STOP_AFTER_CONFLICT=9460" \
  "${prefix}_EVENT_TRACE_HEAP_FROM_DECISION=5795463" \
  "${prefix}_EVENT_TRACE_HEAP_POP_DECISION=5795464" \
  "${prefix}_EVENT_TRACE_HEAP_EACH_BUMP=1" \
  timeout 300 "$binary" "$input"
solver_exit=$?
set -e

cp "$trace" "$campaign/traces/$arm.tsv"
wc -l "$campaign/traces/$arm.tsv"
sha256sum "$campaign/traces/$arm.tsv"
echo "arm=$arm conflict=9459 heap_pop_decision=5795464 solver_exit=$solver_exit expected_exit=$expected_exit"
[[ "$solver_exit" -eq "$expected_exit" ]]
