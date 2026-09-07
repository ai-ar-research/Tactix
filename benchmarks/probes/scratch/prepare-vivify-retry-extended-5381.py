#!/usr/bin/env python3
"""Extend the repaired prefix with the same immutable pins, no new semantics."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5378-vivify-retry'
new = root / 'benchmarks/runs/20260907-5381-vivify-retry-extended'
new.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces'): (new / d).mkdir()
for p in (old / 'pins').iterdir(): shutil.copy2(p, new / 'pins' / p.name)
for p in old.glob('*.patch'): shutil.copy2(p, new / p.name)
source = (old / 'run.py').read_text()
source = '\n'.join(l for l in source.splitlines() if not any(s in l for s in ('_BVE_CANDIDATE_', '_CHOICE_STATE_', '_RETRY_PRODUCER_'))) + '\n'
source = source.replace('"345034"', '"1000000"')
(new / 'run.py').write_text(source)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)))
m = json.loads((old / 'manifest.json').read_text())
m.update(purpose='Extend repaired decision/boundary prefix to one million conflicts.',
         hypothesis='After retry-advice closure, locate the next worse boundary if a decision split remains.',
         acceptance='Two clean zero stops; native decision bytes equal 5373, each arm preserves its 5378 prefix. Compare first changed decision and prior normalized boundaries.',
         parent_campaign=str(old.relative_to(root)), source_note='Exact existing 5378 pins, no build or semantic edit. Stopped diagnostic, not a timing/verdict gate.')
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new / 'pins').iterdir(), *new.glob('*.patch'), new / 'run.py', new / 'array.sbatch']}
(new / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (new / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
