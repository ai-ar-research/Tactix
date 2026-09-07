#!/usr/bin/env python3
"""Locate the first portable split on the largest checked current runtime gap."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5381-vivify-retry-extended'
c = root / 'benchmarks/runs/20260907-5396-rank27-prefix'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'):
    (c / d).mkdir()
for name, path in [('previous', old / 'pins/tactix'), ('kissat', old / 'pins/kissat'),
                   ('candidate', root / 'build/pin-5391-eliminate-dense-entry/tactix')]:
    shutil.copy2(path, c / 'pins' / name)
shutil.copy2(root / 'benchmarks/probes/scratch/rank27-prefix-task-5396.py', c / 'run.py')
script = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('--array=0-1', '--array=0-2')
(c / 'array.sbatch').write_text(script)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
input_path = 'benchmarks/cache/tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz'
m = dict(parent_campaign=str(old.relative_to(root)),
         candidate_authority='benchmarks/runs/20260907-5391-eliminate-dense-entry/manifest.json',
         source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',
         candidate_source='.claude/worktrees/runtime-eliminate-dense-entry-5391',
         input=input_path, input_sha256_compressed=sha(root / input_path), stop_conflict=25000,
         hypothesis='The largest checked full-view gap (Rank 27, 499.203 versus 244.873 seconds in 5383) includes substantially more search work. Locate first decision/boundary divergence and whether dense entry advances or removes it before selecting a repair.',
         validation='Three clean intentional exit-zero prefix stops, immutable hashes, complete decision/boundary streams and deleted partial certificates. Normalize portable fields; unequal raw counts alone do not establish weaker semantics.',
         contract='Full-width 0–2, one bound proof-on solver per task, 12 logical CPUs/48000 MiB, no throttle/node constraint. Diagnostic time is not acceptance.',
         decision_columns=['conflicts', 'decisions', 'level', 'external_literal', 'warming'],
         hashes={str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']})
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
