#!/usr/bin/env python3
"""Repeat the selected witness without irrelevant per-heap-move selection."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5413-rank35-warmup-implications'
c = root / 'benchmarks/runs/20260907-5415-rank35-warmup-implications'
authority = root / 'benchmarks/runs/20260907-5414-imply-fast/manifest.json'
a = json.loads(authority.read_text())
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
for arm, binary in zip(('candidate', 'kissat'), a['binaries']): shutil.copy2(root / binary, c / 'pins' / arm)
(c / 'run.py').write_text((old / 'run.py').read_text().replace("'tactix-5413-'", "'tactix-5415-'"))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)), candidate_authority=str(authority.relative_to(root)),
         candidate_source='.claude/worktrees/runtime-imply-fast-5414',
         supersession='5413 retained full per-heap-move selected-variable tests even in implication-only mode; this observer rejects those helpers immediately.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
