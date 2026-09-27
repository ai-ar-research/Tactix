#!/usr/bin/env bash
#SBATCH --job-name=am495-phase1-score
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=4096M
#SBATCH --time=00:30:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am495-phase1-score/score-%j.log
set -u
cd /home/hwu/SAT-solving
python3 - <<'PY'
import hashlib
from pathlib import Path
pins = {
    'tools/bench/fixed_runtime.py': 'ba0b2a07a12d63df25df1e7cf7409e2b24d7896257016173e9eaf956fada3be8',
    'benchmarks/runs/20260927-am487-rank1-50/manifest.json': '5459b235e5051f74bff51dedd47af481b0aefc7195f70cb84a3896fae6ab12fc',
    'benchmarks/runs/20260927-am487-b28c5/manifest.json': '37a23e60c31d9912c67e68e46614829230f26609a905005ecc44f673c1f58e39',
}
for name, expected in pins.items():
    digest = hashlib.sha256(Path(name).read_bytes()).hexdigest()
    if digest != expected:
        raise SystemExit(f'pin changed: {name}')
print('scorer and manifest hashes: OK')
PY
if [ "$?" -ne 0 ]; then exit 2; fi
out=benchmarks/probes/scratch/am495-phase1-score
rank_status=0
python3 tools/bench/fixed_runtime.py score benchmarks/runs/20260927-am487-rank1-50 > "$out/rank-score.json" 2> "$out/rank-score.err" || rank_status=$?
b28_status=0
python3 tools/bench/fixed_runtime.py score benchmarks/runs/20260927-am487-b28c5 > "$out/b28-score.json" 2> "$out/b28-score.err" || b28_status=$?
printf 'rank_status=%s\nb28_status=%s\n' "$rank_status" "$b28_status" > "$out/status.txt"
if [ "$rank_status" -ne 0 ] || [ "$b28_status" -ne 0 ]; then exit 1; fi
