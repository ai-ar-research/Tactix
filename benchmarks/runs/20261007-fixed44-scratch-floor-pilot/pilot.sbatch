#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=16G
#SBATCH --time=00:20:00
#SBATCH --no-requeue
#SBATCH --job-name=fixed44-floor-pilot
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-fixed44-scratch-floor-pilot/pilot-%j.log
set -euo pipefail
ulimit -c 0
export PYTHONDONTWRITEBYTECODE=1 TMPDIR=/tmp
P=/home/hwu/SAT-solving/benchmarks/runs/20261007-fixed44-scratch-floor-pilot
cd "$P" && sha256sum -c sources.sha256
df -B1 /tmp
exec python3 "$P/pilot.py" "$P/snapshot" "$P/evidence-$SLURM_JOB_ID.json"
