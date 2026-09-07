#!/usr/bin/env python3
"""Pin macro/root boundary watch-list observer on the core-only candidate."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5321-all-root-order'
new = root / 'benchmarks/runs/20260907-5323-binary-watch-boundary'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
shutil.copy2(root / 'build/pin-5323-binary-watch/tactix', new / 'pins/tactix')
shutil.copy2(root / 'tools/solvers/variants/kissat-binary-watch-5323/build/kissat', new / 'pins/kissat')
shutil.copy2(old / 'candidate.patch', new / 'candidate.patch')
for arm in ('tactix', 'kissat'):
    shutil.copy2(root / f'benchmarks/probes/scratch/binary-watch-{arm}-5323.patch', new / f'{arm}-observer.patch')
(new / 'run.py').write_text((old / 'run.py').read_text().replace(
    '"SAT_EVENT_TRACE_ALL_ROOTS": "1",',
    '"SAT_EVENT_TRACE_ALL_ROOTS": "1",\n            "SAT_BINARY_WATCH_LITERAL": "562",'))
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)).replace('all-root-order', 'binary-watch-boundary'))
previous = json.loads((old / 'manifest.json').read_text())
manifest = {k: previous[k] for k in ('source_commit', 'source_patch', 'input_sha256_compressed', 'contract', 'kissat_source', 'acceptance')}
manifest.update(
    purpose='UNFUZZED core-only source: observe binary owner 562 at macro boundaries/root assignments, comparing physical watches and the dense ledger.',
    reference_campaign=str(old.relative_to(root)),
    hashes={str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*(new / 'pins').iterdir(), new / 'candidate.patch', new / 'run.py', new / 'array.sbatch',
                      new / 'tactix-observer.patch', new / 'kissat-observer.patch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
