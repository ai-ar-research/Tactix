#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=16384M
#SBATCH --time=00:12:00
#SBATCH --array=0-1
#SBATCH --no-requeue
#SBATCH --job-name=r401-policy-build
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-rank401-reference-policy-causal/logs/build-%A_%a.log
set -euo pipefail
ulimit -c 0
campaign=/home/hwu/SAT-solving/benchmarks/runs/20261007-rank401-reference-policy-causal
archive=/home/hwu/SAT-solving/benchmarks/runs/20261007-cumulative-37226faa7-graph-preparation/source.tar
test "$(sha256sum "$archive" | cut -d' ' -f1)" = c11109ff8f2358406e48b1f12ba584cfd810bb1c5d48c61257c304f2ce0fd2ea
scratch="$campaign/build-$SLURM_JOB_ID-$SLURM_ARRAY_TASK_ID"
mkdir -p "$scratch/source"
trap 'rm -rf -- "$scratch"' EXIT
tar -xf "$archive" -C "$scratch/source"
python3 - "$scratch/source" "$SLURM_ARRAY_TASK_ID" <<'PATCH'
from pathlib import Path
import sys
r=Path(sys.argv[1]);mode=int(sys.argv[2]);p=r/'src/policies/structured_root_strategy.cpp';s=p.read_text();a='  group.base_selection = SymmetryBaseSelection::FirstAmoThenForwardRemainder;';assert s.count(a)==1;p.write_text(s.replace(a,a+'\n  group.cut_point = SymmetryCutPoint::LargestOrbitLast;'))
p=r/'src/policies/default_strategy_config.hpp';s=p.read_text();a='bool structured_root_residual_search_handoff = true;';assert s.count(a)==1;s=s.replace(a,'bool structured_root_residual_search_handoff = false;')
if mode==0:
 a='bool initial_fast_elimination = true;';assert s.count(a)==1;s=s.replace(a,'bool initial_fast_elimination = false;')
p.write_text(s)
PATCH
cmake -S "$scratch/source" -B "$scratch/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc -DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/c++ -DTACTIX_ENABLE_EVENT_TRACE=OFF -DTACTIX_ENABLE_PROOF_SHADOW=OFF -DTACTIX_ENABLE_RULE_JOURNAL=OFF >"$campaign/results/configure-$SLURM_ARRAY_TASK_ID.log" 2>&1
cmake --build "$scratch/build" --parallel 4 --target tactix >"$campaign/results/build-$SLURM_ARRAY_TASK_ID.log" 2>&1
install -m0550 "$scratch/build/tactix" "$campaign/pins/cut-last-original-fastel$SLURM_ARRAY_TASK_ID"
sha256sum "$campaign/pins/cut-last-original-fastel$SLURM_ARRAY_TASK_ID" >"$campaign/results/fastel$SLURM_ARRAY_TASK_ID.sha256"
