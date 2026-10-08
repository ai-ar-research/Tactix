#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=8
#SBATCH --mem=32000M
#SBATCH --time=02:30:00
#SBATCH --no-requeue
#SBATCH --job-name=r569-kissat-stats
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261008-r569-kissat-stats/logs/%x-%j.log
# Statistics-only rebuild of kissat-sc2024 1e56df6 (build flags minus -DQUIET).
# Reference-solver exception: CNF and proof live in node-local scratch and are deleted;
# the proof hash must equal the pinned binary's 055e6bd8... to bind the statistics.
set -euo pipefail
ulimit -c 0
W="${SLURM_TMPDIR:-/tmp}/r569-kstats-${SLURM_JOB_ID}"
mkdir -p "$W"; trap '/bin/rm -rf "$W"' EXIT
xz -dc /home/hwu/SAT-solving/benchmarks/cache/tierD24-26/778e0fd8f034b7cfd539b1b523ed4c4a-lec_mult_CvW_12x12.sanitized.cnf.xz > "$W/input.cnf"
set +e
timeout 5400 /usr/bin/time -v /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-kissat-stats/kissat-sc2024-stats "$W/input.cnf" "$W/proof.drat" > /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-kissat-stats/results/kissat-stats.stdout 2> /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-kissat-stats/results/kissat-stats.time
code=$?
set -e
echo "exit=$code" > /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-kissat-stats/results/kissat-stats.exit
stat -c %s "$W/proof.drat" >> /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-kissat-stats/results/kissat-stats.exit
sha256sum "$W/proof.drat" | awk '{print $1}' >> /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-kissat-stats/results/kissat-stats.exit
