#!/usr/bin/env python3
"""Trace the second-largest complete bound-candidate loss on immutable pins."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260907-5463-rank37-bound-prefix'
c = runs / '20260908-5479-rank32-bound-prefix'
losses = json.loads((runs / '20260907-5465-bound-advance-ranks1-40/next-losses.json').read_text())
witness = losses['ranked_losses'][1]
assert witness['rank'] == 32
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text((old / 'run.py').read_text().replace("'tactix-5463-'", "'tactix-5479-'"))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(parent_campaign=str(old.relative_to(root)), input='benchmarks/cache/' + witness['instance'],
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    hypothesis='Rank 32 is the second-largest full 5465 loss: 421.021/274.413 seconds, conflicts 1.367353x and propagations 1.263398x. Locate the first portable boundary before selecting a general repair.',
    selection_authority='benchmarks/runs/20260907-5465-bound-advance-ranks1-40/next-losses.json',
    validation='Two clean 100000-conflict controlled stops, complete decision streams from zero, prefix and pin/trace hashes, and cleanup. Interpret divergences causally, not as automatic defects.',
    limitation='Diagnostic-only partial proof; no checked complete verdict or acceptance timing. Does not replace Rank 1-40 or B28+C5.')
m['input_sha256_compressed'] = sha(root / m['input'])
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
