#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=8
#SBATCH --mem=128000M
#SBATCH --time=14:00:00
#SBATCH --no-requeue
#SBATCH --job-name=r569-kissat-proof-mem
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/logs/%x-%j.log
# Reference-solver/checker memory measurement (stated exception to the runtime-task rule):
# the CNF copy, DRAT proof and LSR outputs stay in node-local scratch and are deleted;
# only kissat's statistics log, JSON stats and hashes persist.
set -euo pipefail
ulimit -c 0
W="${SLURM_TMPDIR:-/tmp}/r569-kissat-${SLURM_JOB_ID}"
mkdir -p "$W"
trap '/bin/rm -rf "$W"' EXIT
CNF=/home/hwu/SAT-solving/benchmarks/cache/tierD24-26/778e0fd8f034b7cfd539b1b523ed4c4a-lec_mult_CvW_12x12.sanitized.cnf.xz
KISSAT=/home/hwu/SAT-solving/benchmarks/baselines/sc24-26-32g-5000s/pins/kissat-sc2024
xz -dc "$CNF" > "$W/input.cnf"
start=$(date +%s.%N)
set +e
timeout 5000 /usr/bin/time -v "$KISSAT" "$W/input.cnf" "$W/proof.drat" > /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/results/kissat569.stdout 2> /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/results/kissat569.time
code=$?
set -e
end=$(date +%s.%N)
echo "exit=$code wall=$(echo "$end - $start" | bc)" > /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/results/kissat569.exit
ls -l "$W/proof.drat" >> /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/results/kissat569.exit
sha256sum "$W/proof.drat" | awk '{print $1}' >> /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/results/kissat569.exit
/bin/rm -f "$W/input.cnf"
if [ "$code" -eq 20 ]; then
  python3 -I /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/measure.py --cnf "$CNF" --proof "$W/proof.drat" --out /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/results/kissat569-checkers.json     --scratch "$W/measure" --stats /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/proofstats --timeout 36000     --checker memrep64=/home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/dsr-trim64-memrep --checker memrep32=/home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/dsr-trim32-memrep
fi
