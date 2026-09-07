#!/usr/bin/env python3
"""Test bridge pose at the unchanged predeclared later decision boundary."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5319-core-decision-window'
pin = root / 'benchmarks/runs/20260907-5327-bridge-root-order'
new = root / 'benchmarks/runs/20260907-5328-bridge-decision-window'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
for arm in ('tactix', 'kissat'):
    shutil.copy2(pin / 'pins' / arm, new / 'pins' / arm)
for name in ('candidate.patch', 'tactix-observer.patch', 'kissat-observer.patch'):
    shutil.copy2(pin / name, new / name)
for name in ('run.py', 'compare.py'):
    shutil.copy2(old / name, new / name)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)).replace('core-decision-window', 'bridge-decision-window'))
previous = json.loads((pin / 'manifest.json').read_text())
manifest = {k: previous[k] for k in ('source_commit', 'source_patch', 'input_sha256_compressed', 'contract', 'kissat_source', 'release_sha256')}
manifest.update(
    purpose='UNFUZZED 5327 bridge pose: does closing root propagation order also close the conflict-75451 warmup decision witness?',
    reference_campaign=str(pin.relative_to(root)),
    acceptance='No truncation; both traces reach 75452; compare ordinary and warmup decisions separately in conflicts 50000..75451; verify scratch deletion. Not a runtime or correctness gate.',
    hashes={str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*(new / 'pins').iterdir(), new / 'candidate.patch', new / 'run.py', new / 'array.sbatch',
                      new / 'tactix-observer.patch', new / 'kissat-observer.patch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(new)
