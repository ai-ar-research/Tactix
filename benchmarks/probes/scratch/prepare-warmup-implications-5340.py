#!/usr/bin/env python3
"""Resolve whether the first warm-up choice follows earlier propagation drift."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5328-bridge-decision-window'
new = root/'benchmarks/runs/20260907-5340-warmup-implications'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new/name).mkdir()
for p in (old/'pins').iterdir():
    if p.is_file():
        shutil.copy2(p, new/'pins'/p.name)
for p in old.glob('*.patch'):
    shutil.copy2(p, new/p.name)
source = (old/'run.py').read_text().replace('prefix + "_EVENT_TRACE_TRAJECTORY_ONLY": "1",',
    'prefix + "_EVENT_TRACE_SKIP_HEAP": "1",\n            prefix + "_EVENT_TRACE_SKIP_WATCH": "1",')
source = source.replace('"_EVENT_TRACE_CONFLICT_FROM": "50000"', '"_EVENT_TRACE_CONFLICT_FROM": "75451"')
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(new)))
m = json.loads((old/'manifest.json').read_text())
m.pop('job', None)
m.update(purpose='UNFUZZED detailed Rank-34 warm-up decision and implication frontier at conflict 75451; not timing or correctness acceptance.',
         hypothesis='Selected heap zero-score tie may be downstream of different propagation membership or order. Find the first implication/reason difference before the level-24 decision.',
         acceptance='Stop at 75452 without truncation and scratch clean; compare warm-up decisions and ordered implication literals/reasons. Normalize root trail offsets, not actual non-root decisions.',
         parent_campaign=str(old.relative_to(root)))
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(new/'pins').iterdir(), *new.glob('*.patch'), new/'run.py', new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
