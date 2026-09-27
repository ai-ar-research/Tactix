#!/usr/bin/env bash
#SBATCH --job-name=am512-wv100-score
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=01:00:00
#SBATCH --dependency=afterany:1731686
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am512-wv100-dependent-score/out/%j.score.out
#SBATCH --error=/home/hwu/SAT-solving/benchmarks/probes/scratch/am512-wv100-dependent-score/out/%j.score.err
set -euo pipefail
root=/home/hwu/SAT-solving
campaign="$root/benchmarks/runs/20260927-am507-wv100-dsr-trim-first"
output="$root/benchmarks/probes/scratch/am512-wv100-dependent-score"
test "$(sha256sum "$campaign/manifest.json" | cut -d' ' -f1)" = ea870c4ffa1409311a99e535bc471621ca6d3e19c0b671a42ef88619f343438c
test "$(sha256sum "$campaign/score.py" | cut -d' ' -f1)" = 378a63d885c7b91caef1cfdf218401027b17127208ccbbab34e6b0c6fc31b93b
python3 "$campaign/score.py" > "$output/score.json.tmp"
mv "$output/score.json.tmp" "$output/score.json"
cat "$output/score.json"
