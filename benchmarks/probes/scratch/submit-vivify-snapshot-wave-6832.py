#!/usr/bin/env python3
"""Submit one predeclared wave after exact prior solves; proofs asynchronous."""
from pathlib import Path
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from checked_comparison import classify
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'), '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
for folder in ('provisional', 'results'):
    for p in (root/'benchmarks/runs/20260910-6828-vivify-snapshot-BC-gate'/folder).glob('*.json'):
        state = classify(json.loads(p.read_text()), folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
source = Path(__file__).with_name('submit-collapse-wave-6333.py')
code = source.read_text()
before = '20260909-6332-collapse-three-wave-runtime'
assert code.count(before) == 1
code = code.replace(before, '20260910-6831-vivify-snapshot-runtime')
before = "for rel,digest in m['pin_sha256'].items():assert sha(c/rel)==digest"
assert before in code
code = code.replace(before, before + "\nfor rel,digest in m['dependencies'].items():assert sha(root/rel)==digest")
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
