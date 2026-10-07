#!/usr/bin/env bash
#SBATCH --job-name=r569-cube-verify
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --time=00:05:00
#SBATCH --array=0-1
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-rank569-frontier-cube-gate/logs/verify-%A_%a.log
#SBATCH --no-requeue
set -euo pipefail
ROOT=/home/hwu/SAT-solving
RUN="$ROOT/benchmarks/runs/20261007-rank569-frontier-cube-gate"
CUBE=$((3 * SLURM_ARRAY_TASK_ID))
set +e
"$RUN/pins/drat-trim" "$RUN/pins/input.cnf" \
  "$RUN/results/case-$CUBE.drat" -f -U \
  > "$RUN/results/case-$CUBE.recheck.stdout" \
  2> "$RUN/results/case-$CUBE.recheck.stderr"
CHECK_RC=$?
set -e
printf '%s\n' "$CHECK_RC" > "$RUN/results/case-$CUBE.recheck.exit"
if [[ "$CHECK_RC" -ne 1 ]] || \
   ! grep -q 'VERIFIED derivation: all lemmas preserve satisfiability' "$RUN/results/case-$CUBE.recheck.stdout" || \
   ! grep -q '^s DERIVATION' "$RUN/results/case-$CUBE.recheck.stdout"; then
  exit 20
fi
