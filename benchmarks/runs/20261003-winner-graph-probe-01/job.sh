#!/bin/bash
set -euo pipefail
cd /home/hwu/SAT-solving
export CLAUDE_JOB_DIR=/home/hwu/SAT-solving/benchmarks/runs/20261003-winner-graph-probe-01
export LD_LIBRARY_PATH=/cm/local/apps/gcc/13.1.0/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
exec python3 benchmarks/probes/scratch/run_graph_profile_probe.py benchmarks/runs/20261003-winner-graph-probe-01
