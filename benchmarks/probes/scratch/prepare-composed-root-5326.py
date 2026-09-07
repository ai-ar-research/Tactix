#!/usr/bin/env python3
"""Test sequential rewrites after certifying all pairs on the original image."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5324-binary-producers'
new = root / 'benchmarks/runs/20260907-5326-composed-root-order'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
shutil.copy2(root / 'build/pin-5326-composed-trace/tactix', new / 'pins/tactix')
shutil.copy2(old / 'pins/kissat', new / 'pins/kissat')
shutil.copy2(root / 'benchmarks/probes/scratch/composed-core-release-5326.patch', new / 'candidate.patch')
for name in ('tactix-observer.patch', 'kissat-observer.patch', 'run.py'):
    shutil.copy2(old / name, new / name)
shutil.copy2(root / 'benchmarks/runs/20260907-5321-all-root-order/compare.py', new / 'compare.py')
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)).replace('binary-producers', 'composed-root-order'))
previous = json.loads((old / 'manifest.json').read_text())
manifest = {k: previous[k] for k in ('source_commit', 'source_patch', 'input_sha256_compressed', 'contract', 'kissat_source', 'acceptance')}
manifest.update(
    purpose='UNFUZZED composed ablation: original-core units, sequential connected rewrites after all pair proofs, and physical binary-order publication. Test interaction on the first root mismatch.',
    release_sha256=hashlib.sha256((root / 'build/pin-5326-composed/tactix').read_bytes()).hexdigest(),
    focused_validation='test_sweep, test_substitute, test_subsolver and 44 independent DRAT/LRAT proof cases pass.',
    reference_campaign=str(old.relative_to(root)),
    hashes={str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*(new / 'pins').iterdir(), new / 'candidate.patch', new / 'run.py', new / 'array.sbatch',
                      new / 'tactix-observer.patch', new / 'kissat-observer.patch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
