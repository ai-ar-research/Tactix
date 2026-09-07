#!/usr/bin/env python3
"""Freeze the source-backed empty Factor advice repair as an isolated ablation."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5353-factor-stamps'
new = root / 'benchmarks/runs/20260907-5354-empty-factor'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
shutil.copy2(root / 'build/pin-5354-empty-factor/tactix', new / 'pins/tactix')
shutil.copy2(old / 'pins/kissat', new / 'pins/kissat')
for p in old.glob('*.patch'): shutil.copy2(p, new / p.name)
(new / 'run.py').write_text((old / 'run.py').read_text())
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)))
patch = ''
for rel in ('src/rules/factor/factor.cpp', 'src/policies/heuristics.cpp',
            'src/policies/heuristics.hpp', 'tests/test_factor.cpp'):
    r = subprocess.run(['diff', '-u', '--label', 'a/' + rel, '--label', 'b/' + rel,
        root / '.claude/worktrees/runtime-factor-stamps-5353' / rel,
        root / '.claude/worktrees/runtime-empty-factor-5354' / rel], capture_output=True, text=True)
    assert r.returncode == 1
    patch += r.stdout
(new / 'empty-factor.patch').write_text(patch)
m = json.loads((old / 'manifest.json').read_text()); m.pop('job', None)
m.update(purpose='UNFUZZED isolated empty Factor advice repair, not production or timing gate.',
    hypothesis='Publishing an empty completed-pass advice interval and consuming its queue restamp restores native numeric relevance lifecycle.',
    acceptance='Stop zero before BVE at 95799, clean scratch. Compare all Factor housekeeping boundaries and complete initial BVE membership, counts, scores and order; record any earlier changed trajectory.',
    parent_campaign=str(old.relative_to(root)), source_note='Fresh 5353 Tactix copy plus empty-factor.patch; exact native 5353 pin. No new strategy value, guard, proof or reconstruction change.')
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
    for p in [*(new / 'pins').iterdir(), *new.glob('*.patch'), new / 'run.py', new / 'array.sbatch']}
(new / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (new / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
