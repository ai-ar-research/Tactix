#!/usr/bin/env bash
#SBATCH --job-name=r569-cube-cases
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=00:10:00
#SBATCH --array=0-3
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-rank569-frontier-cube-gate/logs/cases-%A_%a.log
#SBATCH --no-requeue
set -euo pipefail
ROOT=/home/hwu/SAT-solving
RUN="$ROOT/benchmarks/runs/20261007-rank569-frontier-cube-gate"
INPUT="$ROOT/benchmarks/cache/tierD24-26/778e0fd8f034b7cfd539b1b523ed4c4a-lec_mult_CvW_12x12.sanitized.cnf.xz"
PROBE="$RUN/pins/frontier_cube_probe"
CHECKER="$RUN/pins/drat-trim"
CASE="$SLURM_ARRAY_TASK_ID"
SCRATCH="${SLURM_TMPDIR:-/tmp}/r569-cube-$SLURM_ARRAY_JOB_ID-$CASE"
mkdir -p "$SCRATCH" "$RUN/results"
xz -dc "$INPUT" > "$SCRATCH/input.cnf"
PROOF="$RUN/results/case-$CASE.drat"
RAW="$RUN/results/case-$CASE.raw.json"
set +e
/usr/bin/time -f 'wall=%e rss_kb=%M' -o "$RUN/results/case-$CASE.time" \
  "$PROBE" "$SCRATCH/input.cnf" 3411 9 21 "$CASE" 100000000 67108864 \
  "$PROOF" "$RAW"
PROBE_RC=$?
set -e
if [[ "$PROBE_RC" -ne 0 ]]; then
  rm -rf "$SCRATCH"
  exit "$PROBE_RC"
fi
set +e
"$CHECKER" "$SCRATCH/input.cnf" "$PROOF" -f -U \
  > "$RUN/results/case-$CASE.check.stdout" \
  2> "$RUN/results/case-$CASE.check.stderr"
CHECK_RC=$?
set -e
if [[ "$CHECK_RC" -ne 1 ]] || \
   ! grep -q 'VERIFIED derivation: all lemmas preserve satisfiability' "$RUN/results/case-$CASE.check.stdout" || \
   ! grep -q '^s DERIVATION' "$RUN/results/case-$CASE.check.stdout"; then
  rm -rf "$SCRATCH"
  exit 20
fi
python3 - "$RAW" "$PROOF" "$RUN/results/case-$CASE.json" <<'PY'
import hashlib,json,pathlib,sys
raw,proof,out=map(pathlib.Path,sys.argv[1:])
row=json.loads(raw.read_text())
h=hashlib.sha256(proof.read_bytes()).hexdigest()
row.update(checker_verified=True,proof=str(proof),proof_sha256=h)
out.write_text(json.dumps(row,indent=2,sort_keys=True)+'\n')
PY
rm -rf "$SCRATCH"
