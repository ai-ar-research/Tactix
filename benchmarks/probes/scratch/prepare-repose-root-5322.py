#!/usr/bin/env python3
"""Compare physical-watch publication at the first remaining root boundary."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5321-all-root-order'
new = root / 'benchmarks/runs/20260907-5322-reposed-root-order'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
shutil.copy2(root / 'build/pin-5322-repose-trace/tactix', new / 'pins/tactix')
shutil.copy2(old / 'pins/kissat', new / 'pins/kissat')
shutil.copy2(root / 'benchmarks/probes/scratch/core-repose-release-5322.patch', new / 'candidate.patch')
for name in ('tactix-observer.patch', 'kissat-observer.patch', 'run.py', 'compare.py'):
    shutil.copy2(old / name, new / name)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)).replace('all-root-order', 'reposed-root-order'))
previous = json.loads((old / 'manifest.json').read_text())
manifest = {k: previous[k] for k in ('source_commit', 'source_patch', 'input_sha256_compressed', 'contract', 'kissat_source')}
manifest.update(
    purpose='UNFUZZED: compose original-core exposure with physical binary-watch order publication; test root-order witness.',
    observer_note='The Tactix all-roots patch is identical to 5321, now on the clean Release source (without its earlier unused OBS_QUERY observer).',
    release_sha256=hashlib.sha256((root / 'build/pin-5322-core-repose/tactix').read_bytes()).hexdigest(),
    focused_validation='test_substitute including physical-order fixture, test_sweep, test_subsolver, and all 44 DRAT/LRAT emitted cases pass.',
    reference_campaign=str(old.relative_to(root)),
    acceptance=previous['acceptance'],
    hashes={str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*(new / 'pins').iterdir(), new / 'candidate.patch', new / 'run.py', new / 'array.sbatch',
                      new / 'tactix-observer.patch', new / 'kissat-observer.patch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
