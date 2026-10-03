#!/bin/bash
#SBATCH --job-name=ramsey-recognized-dsr
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=32G
#SBATCH --time=01:10:00
#SBATCH --array=0-2
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261003-ramsey-recognized-certificate/slurm-%A_%a.log
set -euo pipefail
cd /home/hwu/SAT-solving
export CLAUDE_JOB_DIR=/home/hwu/SAT-solving/benchmarks/runs/20261003-ramsey-recognized-certificate
export LD_LIBRARY_PATH=/cm/local/apps/gcc/13.1.0/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
exec python3 "$CLAUDE_JOB_DIR/run.py"
