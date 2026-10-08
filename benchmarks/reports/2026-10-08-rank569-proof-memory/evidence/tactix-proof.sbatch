#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task=8
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/logs/%x-%j.log
# Checker memory measurement on a runner-retained Tactix certificate (stated
# checker-measurement exception). Args: TASK (retained-proofs/<TASK>.dsr), SET (all|memrep).
set -euo pipefail
ulimit -c 0
TASK=$1; SET=$2
C=/home/hwu/SAT-solving/benchmarks/runs/20261008-r569-retained-proofs
CNF=/home/hwu/SAT-solving/benchmarks/cache/$(awk -F'\t' -v t="$TASK" '$1==t+0 && NR>1 {print $5}' $C/matrix.tsv)
PROOF=$C/retained-proofs/$TASK.dsr
CK=(--checker memrep64=/home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/dsr-trim64-memrep --checker memrep32=/home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/dsr-trim32-memrep)
if [ "$SET" = all ]; then
  CK+=(--checker pinned32=/home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/pins/dsr-trim-canonical32 --checker pinned64fixed=/home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/pins/dsr-trim-long64fixed)
fi
python3 -I /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/measure.py --cnf "$CNF" --proof "$PROOF" --out /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/results/tactix-$TASK-checkers.json   --scratch "${SLURM_TMPDIR:-/tmp}/r569-mem-${SLURM_JOB_ID}" --stats /home/hwu/SAT-solving/benchmarks/runs/20261008-r569-checker-memory/proofstats --timeout 40000 "${CK[@]}"
