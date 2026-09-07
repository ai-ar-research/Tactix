#!/usr/bin/env python3
"""Fresh reference observer with warm-up implication records."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5340-warmup-implications'
new = root/'benchmarks/runs/20260907-5341-warmup-reference'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new/name).mkdir()
for p in (old/'pins').iterdir():
    if p.is_file():
        shutil.copy2(p, new/'pins'/p.name)
for p in old.glob('*.patch'):
    shutil.copy2(p, new/p.name)
(new/'pins/kissat').chmod(0o644)
shutil.copy2(root/'tools/solvers/variants/kissat-warmup-imply-5341/build/kissat', new/'pins/kissat')
shutil.copy2(old/'run.py', new/'run.py')
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(new)))
rel = 'src/warmup.c'
result = subprocess.run(['diff', '-u', '--label', 'a/'+rel, '--label', 'b/'+rel,
    root/'tools/solvers/variants/kissat-root-all-5321'/rel,
    root/'tools/solvers/variants/kissat-warmup-imply-5341'/rel], text=True, capture_output=True)
assert result.returncode == 1
(new/'kissat-warmup-observer.patch').write_text(result.stdout)
m = json.loads((old/'manifest.json').read_text())
m.pop('job', None)
m.update(purpose='UNFUZZED matched warm-up implication observer, fixing the missing Kissat warmup.c call in 5340.',
         kissat_source='Exact 5321/5328 reference source plus kissat-warmup-observer.patch; fresh Release build.',
         acceptance='Both stopped clean without trace truncation at 75452. First verify existing decisions unchanged from 5340; then compare ordered warm-up implications/reasons before the level-24 decision.',
         parent_campaign=str(old.relative_to(root)))
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(new/'pins').iterdir(), *new.glob('*.patch'), new/'run.py', new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
