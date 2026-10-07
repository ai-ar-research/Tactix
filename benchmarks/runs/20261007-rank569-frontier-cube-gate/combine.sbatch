#!/usr/bin/env bash
#SBATCH --job-name=r569-cube-combine
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=00:10:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-rank569-frontier-cube-gate/logs/combine-%j.log
#SBATCH --no-requeue
set -euo pipefail
ROOT=/home/hwu/SAT-solving
RUN="$ROOT/benchmarks/runs/20261007-rank569-frontier-cube-gate"
INPUT="$ROOT/benchmarks/cache/tierD24-26/778e0fd8f034b7cfd539b1b523ed4c4a-lec_mult_CvW_12x12.sanitized.cnf.xz"
SCRATCH="${SLURM_TMPDIR:-/tmp}/r569-cube-combine-$SLURM_JOB_ID"
mkdir -p "$SCRATCH"
xz -dc "$INPUT" > "$SCRATCH/input.cnf"
python3 "$RUN/combine.py" --results "$RUN/results" \
  --proof "$RUN/results/combined.drat" \
  --metadata "$RUN/results/combined.json"
set +e
"$RUN/pins/drat-trim" "$SCRATCH/input.cnf" \
  "$RUN/results/combined.drat" -f -U \
  > "$RUN/results/combined.check.stdout" \
  2> "$RUN/results/combined.check.stderr"
CHECK_RC=$?
set -e
if [[ "$CHECK_RC" -ne 1 ]] || \
   ! grep -q 'VERIFIED derivation: all lemmas preserve satisfiability' "$RUN/results/combined.check.stdout" || \
   ! grep -q '^s DERIVATION' "$RUN/results/combined.check.stdout"; then
  rm -rf "$SCRATCH"
  exit 20
fi
rm -rf "$SCRATCH"
