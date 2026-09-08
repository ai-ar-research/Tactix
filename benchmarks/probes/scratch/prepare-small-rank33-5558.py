#!/usr/bin/env python3
"""Trace the new second-largest complete-view loss on the measured5522 source."""
from pathlib import Path
import datetime
import hashlib
import json
import shutil
root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260908-5537-rank32-small-prefix'
selection = runs / '20260908-5535-sweep-small-ranks1-40/next-losses.json'
loss = json.loads(selection.read_text())['ranked_losses'][1]
assert loss['rank'] == 33 and loss['seconds_over_kissat'] > 0
c = runs / '20260908-5558-rank33-small-prefix'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-5537-', 'tactix-5558-'))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(parent_campaign=str(old.relative_to(root)), input='benchmarks/cache/' + loss['instance'],
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    selection_authority=dict(path=str(selection.relative_to(root)), sha256=sha(selection)),
    hypothesis='Current5522 Rank33 is1.543346x Kissat,+167.186s,with1.244617x conflicts and1.228768x propagations. Locate the earliest portable boundary after the general Sweep changes before selecting another fidelity repair.',
    validation='Two clean controlled100000-conflict stops,complete decisions fromzero,all pin/input/trace hashes,prefix consistency and scratch cleanup. No prior Rank32 prefix is an identity control for this different input.',
    test_consideration='Unchanged tested trace pins and runner; new binding selected from the complete40-pair authority. Existing comparator scope and hash/cleanup assertions cover this diagnostic-only extension.')
m.pop('causal_authority', None)
m['input_sha256_compressed'] = sha(root / m['input'])
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
