#!/usr/bin/env python3
"""Fresh immutable B28+C5 replay, preserving 5304's complete matched matrix."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5304-pose-BC-gate'
new = root / 'benchmarks/runs/20260907-5315-pose-r2-BC-gate'
full = root / 'benchmarks/runs/20260907-5314-pose-r2-ranks1-40'
manifest = json.loads((old / 'manifest.json').read_text())
new.mkdir(exist_ok=False)
for name in ('pins', 'claims', 'results', 'provisional', 'logs'):
    (new / name).mkdir()
shutil.copy2(old / 'matrix.tsv', new / 'matrix.tsv')
for name in ('shipped', 'checkmodel', 'drat-trim', 'modelcheck.py'):
    shutil.copy2(old / 'pins' / name, new / 'pins' / name)
shutil.copy2(full / 'pins/tactix', new / 'pins/candidate')
for name in ('runtime_task.py', 'candidate-source.patch'):
    shutil.copy2(full / 'pins' / name, new / 'pins' / name)
hashes = {}
for path in sorted((new / 'pins').iterdir()):
    path.chmod(path.stat().st_mode & ~0o222)
    hashes[str(path.relative_to(new))] = hashlib.sha256(path.read_bytes()).hexdigest()
hashes['matrix.tsv'] = hashlib.sha256((new / 'matrix.tsv').read_bytes()).hexdigest()
manifest['pin_sha256'] = hashes
candidate = manifest['arms']['candidate']
candidate['sha256'] = hashes['pins/candidate']
candidate['source_commit'] = subprocess.check_output(
    ['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
candidate['source_patch'] = 'pins/candidate-source.patch'
candidate['source_patch_sha256'] = hashes[candidate['source_patch']]
manifest['candidate_patch'] = candidate['source_patch']
manifest['candidate_patch_sha256'] = candidate['source_patch_sha256']
manifest['predecessor'] = str(old.relative_to(root))
manifest['purpose'] = ('Recheck complete B28+C5 on the final fuzz-passing r2 binary; '
                       '5304 timings/checks belong to the earlier pose-only pin.')
manifest['fuzz_authority'] = 'benchmarks/reports/2026-09-07-sweep-pose-fuzz.md'
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
batch = (old / 'array.sbatch').read_text().replace(str(old), str(new)).replace(
    '5304-pose-BC-gate', '5315-pose-r2-BC-gate')
(new / 'array.sbatch').write_text(batch)
shutil.copy2(old / 'aggregate.py', new / 'aggregate.py')
print(new)
