#!/usr/bin/env python3
from pathlib import Path
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from checked_comparison import classify
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
for folder in ('provisional', 'results'):
    for p in (root/'benchmarks/runs/20260909-6766-watermark-layout-BC-gate'/folder).glob('*.json'):
        status = classify(json.loads(p.read_text()), folder == 'results')
        assert status['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, status)
source = Path(__file__).with_name('submit-collapse-wave-6333.py')
code = source.read_text()
for before, after in {
    '20260909-6332-collapse-three-wave-runtime': '20260909-6770-watermark-layout-runtime',
    '6*wave': '4*wave', '4*wave+5': '4*wave+3', '4*wave+6': '4*wave+4'}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
