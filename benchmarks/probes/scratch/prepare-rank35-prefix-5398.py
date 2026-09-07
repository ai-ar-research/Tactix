#!/usr/bin/env python3
"""Trace the largest remaining checked family witness with existing pins."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5396-rank27-prefix'
c = root / 'benchmarks/runs/20260907-5398-rank35-prefix'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'):
    (c / d).mkdir()
for arm in ('candidate', 'kissat'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace("('previous', 'candidate', 'kissat')", "('candidate', 'kissat')")
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('--array=0-2', '--array=0-1'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)), stop_conflict=100000,
         input='benchmarks/cache/tierD24-26/5c47810953b22baa5f6f45585430fc77-oski15a01b20s_opt.cnf.xz',
         hypothesis='Rank 35 has a checked 558.714 versus 306.599 second gap in 5383, with 8.68 versus 4.75 million conflicts. Locate first portable decision/boundary split on the current isolated candidate; no policy change or timing claim.',
         validation='Two clean intentional exit-zero stops, immutable hashes and complete streams; partial certificates hashed and deleted.',
         contract='Full-width 0–1, one bound proof-on solver per task, 12 logical CPUs/48000 MiB, no throttle/node constraint. Diagnostic time is not acceptance.')
m['input_sha256_compressed'] = sha(root / m['input'])
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
