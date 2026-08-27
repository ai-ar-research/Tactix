#!/usr/bin/env bash
#SBATCH --job-name=r27-current-collect-observer
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --nodelist=node018
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=00:10:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20260826-2200-rank27-current-collection-observer/logs/%j.log

set -euo pipefail
campaign=/home/hwu/SAT-solving/benchmarks/runs/20260826-2200-rank27-current-collection-observer
input="$campaign/rank27-${SLURM_JOB_ID}.cnf"
profile="$campaign/profile-${SLURM_JOB_ID}.data"
trap 'rm -f "$input"' EXIT
xz -dc /home/hwu/SAT-solving/benchmarks/cache/tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz > "$input"
sha256sum "$campaign/pins/tactix-observer" "$input"
ulimit -v 16777216
set +e
env TACTIX_STOP_AFTER_CONFLICT=9450 \
  timeout 300 perf record -F 199 -e cycles:u -o "$profile" -- \
    "$campaign/pins/tactix-observer" "$input"
status=$?
set -e
echo "profiler=perf solver_exit=$status"
exit 0
