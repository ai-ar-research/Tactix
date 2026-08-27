#!/usr/bin/env bash
#SBATCH --job-name=r27-current-broad-callgraph
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --nodelist=node018
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260826-2210-rank27-current-broad-search-callgraph/logs/%j.log

set -euo pipefail
campaign=/home/hwu/SAT-solving/benchmarks/runs/20260826-2210-rank27-current-broad-search-callgraph
scratch=$(mktemp -d "/tmp/r27-current-broad-${SLURM_JOB_ID}.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT
input="$scratch/rank27.cnf"
profile="$scratch/tactix.perf.data"
xz -dc /home/hwu/SAT-solving/benchmarks/cache/tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz > "$input"
allowed=$(awk '/Cpus_allowed_list/ {print $2}' /proc/self/status)
first_cpu=${allowed%%[-,]*}
echo "node=$(hostname) allowed_cpus=$allowed bound_cpu=$first_cpu"
sha256sum "$campaign/pins/tactix-current-dwarf" "$input"
ulimit -v 16777216
set +e
/usr/bin/time -v taskset -c "$first_cpu" perf record -F 199 -e cycles:u \
  --call-graph dwarf -o "$profile" -- \
  env TACTIX_STOP_AFTER_CONFLICT=9450 timeout 300 \
    "$campaign/pins/tactix-current-dwarf" "$input"
solver_exit=$?
set -e
perf report --stdio --no-children --percent-limit 0.10 --sort symbol \
  -i "$profile" > "$campaign/profile/tactix.report.txt"
perf report --stdio --percent-limit 0.10 --sort symbol \
  -i "$profile" > "$campaign/profile/tactix.children.txt"
perf script --header-only -i "$profile" > "$campaign/profile/tactix.header.txt"
cp "$profile" "$campaign/profile/tactix.perf.data"
sha256sum "$campaign/profile/tactix.perf.data"
echo "stop_after_conflict=9450 solver_exit=$solver_exit"
[[ "$solver_exit" -eq 0 ]]
