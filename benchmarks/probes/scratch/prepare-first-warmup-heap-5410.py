#!/usr/bin/env python3
"""Capture the first warm-up decision using the validated immutable observer."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5408-rank35-pre-rephase-heap'
c = root / 'benchmarks/runs/20260907-5410-rank35-first-warmup-heap'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace("'tactix-5408-'", "'tactix-5410-'")
runner = runner.replace("'SAT_EVENT_TRACE_HEAP_SET_CONFLICT': '13560'", "'SAT_EVENT_TRACE_HEAP_SET_CONFLICT': '13563'")
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)), snapshot_conflict=13563,
         hypothesis='Separate intervening conflicts and root restart from the remaining warm-up: snapshot after the first warm-up decision at conflict 13563, ordinary counter 17798663.',
         validation='Two clean stops, unchanged final prefixes and decision streams, same first warm-up row, unique valid snapshot, pin/trace hash and scratch cleanup checks; no timing claim.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
