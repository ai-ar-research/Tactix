#!/bin/bash
set -euo pipefail
cd /home/hwu/SAT-solving
export CLAUDE_JOB_DIR=/home/hwu/SAT-solving/benchmarks/runs/20261003-ramsey-r44-dsr-feasibility
export LD_LIBRARY_PATH=/cm/local/apps/gcc/13.1.0/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
exec python3 benchmarks/probes/scratch/run_ramsey_r44_dsr_probe.py "$CLAUDE_JOB_DIR"
