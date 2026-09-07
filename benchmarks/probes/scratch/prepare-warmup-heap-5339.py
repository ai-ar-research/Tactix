#!/usr/bin/env python3
"""Follow the two exact variables at the remaining Rank-34 warm-up split."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5328-bridge-decision-window'
new = root/'benchmarks/runs/20260907-5339-warmup-heap'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new/name).mkdir()
for p in (old/'pins').iterdir():
    if p.is_file():
        shutil.copy2(p, new/'pins'/p.name)
for p in old.glob('*.patch'):
    shutil.copy2(p, new/p.name)
source = (old/'run.py').read_text()
source = source.replace('prefix + "_EVENT_TRACE_TRAJECTORY_ONLY": "1",',
    'prefix + "_EVENT_TRACE_HEAP_VARS_ONLY": "1",\n            "SAT_EVENT_TRACE_HEAP_VARS": "2733,14194",')
source = source.replace('"_EVENT_TRACE_CONFLICT_FROM": "50000"', '"_EVENT_TRACE_CONFLICT_FROM": "0"')
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(new)))
m = json.loads((old/'manifest.json').read_text())
m.pop('job', None)
m.update(purpose='UNFUZZED two-variable heap lifecycle at the known Rank-34 warm-up divergence; not a timing or correctness gate.',
         hypothesis='The first differing warm-up literal at conflict 75451 is caused by a score, heap-position or liveness difference traceable before its pop.',
         acceptance='Both traces stop at 75452 without truncation, runner scratch clean; compare selected-variable heap operations and state from conflict 0. Matching ordinary decisions is prior 5328 evidence, not inferred from this selector.',
         parent_campaign=str(old.relative_to(root)))
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(new/'pins').iterdir(), *new.glob('*.patch'), new/'run.py', new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
