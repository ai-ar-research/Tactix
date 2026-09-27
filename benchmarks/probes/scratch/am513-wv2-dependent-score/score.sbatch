#!/usr/bin/env bash
#SBATCH --job-name=am513-wv2-score
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=01:00:00
#SBATCH --dependency=afterany:1731864
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am513-wv2-dependent-score/out/%j.score.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/scratch/am513-wv2-dependent-score/out/%j.score.err
set -euo pipefail
root=/home/hwu/SAT-solving
campaign="$root/benchmarks/runs/20260927-am511-wv2-89-dsr-trim-first"
output="$root/benchmarks/probes/scratch/am513-wv2-dependent-score"
test "$(sha256sum "$campaign/manifest.json" | cut -d' ' -f1)" = 36312e72fff10cb0b1dd9552944c85ac46fa50a76e1331016fbb76f9a187586a
test "$(sha256sum "$campaign/score.py" | cut -d' ' -f1)" = 9153e251bbda0f8ae950a369f47d21c3fa2aaca6f2f1c1b9c63fd41fa3c4ee74
python3 "$campaign/score.py" > "$output/score.raw.json"
python3 - "$output/score.raw.json" > "$output/score.json.tmp" <<'PY'
import json
import sys

raw = json.load(open(sys.argv[1]))
assert raw['complete'] is True
assert raw['sat_models'] == 57 and raw['unsat_proofs'] == 32
assert raw['sat_models'] + raw['unsat_proofs'] == 89
# The frozen gate enforces 89/89 but its display field says 100.
assert raw['checked_clean'] == 100
raw['checked_clean'] = 89
raw['frozen_scorer_raw_checked_clean'] = 100
raw['scorer_reporting_correction'] = 'AM513: 100 is a display typo; frozen gate requires all 89 WV2-89 rows'
print(json.dumps(raw, indent=2))
PY
mv "$output/score.json.tmp" "$output/score.json"
cat "$output/score.json"
