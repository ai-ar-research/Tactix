#!/usr/bin/env python3
"""Pin a root-only observer without the older selector's size/cap defects."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5320-core-root-order'
new = root / 'benchmarks/runs/20260907-5321-all-root-order'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
shutil.copy2(root / 'build/pin-5321-core-roots/tactix', new / 'pins/tactix')
shutil.copy2(root / 'tools/solvers/variants/kissat-root-all-5321/build/kissat', new / 'pins/kissat')
shutil.copy2(old / 'candidate.patch', new / 'candidate.patch')
for arm in ('tactix', 'kissat'):
    shutil.copy2(root / f'benchmarks/probes/scratch/all-roots-{arm}-5321.patch', new / f'{arm}-observer.patch')
(new / 'run.py').write_text((old / 'run.py').read_text().replace(
    '"SAT_EVENT_TRACE_HEAP_VARS": ",".join(map(str, range(1, 22501))),',
    '"SAT_EVENT_TRACE_ALL_ROOTS": "1",'))
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)).replace('core-root-order', 'all-root-order'))
previous = json.loads((old / 'manifest.json').read_text())
manifest = {k: previous[k] for k in ('source_commit', 'source_patch', 'input_sha256_compressed', 'contract')}
manifest.update(
    purpose='UNFUZZED candidate: compare all root assignments at conflict 26913, independently of explicit Sweep unit emissions.',
    selection='Diagnostic-only all-roots flag bypasses per-variable selectors and rejects unrelated events before row cap; no solver semantics change.',
    kissat_source='Pristine 8af8e56f174b778aef3aa45af9f739b2a5f492c2 plus tracked c2a95a6 event overlay and kissat-observer.patch; fresh Release build.',
    reference_campaign='benchmarks/runs/20260907-5317-core-unit-candidate',
    acceptance='No truncation, both traces reach 26914, ordered root literals/reasons compared, scratch deleted. Stopped runs are not correctness or timing evidence.',
    hashes={str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*(new / 'pins').iterdir(), new / 'candidate.patch', new / 'run.py', new / 'array.sbatch',
                      new / 'tactix-observer.patch', new / 'kissat-observer.patch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
