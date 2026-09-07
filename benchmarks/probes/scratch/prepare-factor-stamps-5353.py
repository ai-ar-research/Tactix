#!/usr/bin/env python3
"""Observe Factor's no-yield queue housekeeping without changing semantics."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5352-queue-stamps'
new = root / 'benchmarks/runs/20260907-5353-factor-stamps'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
for arm, binary in (('tactix', 'build/pin-5353-factor-stamps/tactix'),
                    ('kissat', 'tools/solvers/variants/kissat-factor-stamps-5353/build/kissat')):
    shutil.copy2(root / binary, new / 'pins' / arm)
for p in old.glob('*.patch'):
    shutil.copy2(p, new / p.name)
(new / 'run.py').write_text((old / 'run.py').read_text())
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)))
for arm, before, after, files in (
    ('tactix', '.claude/worktrees/runtime-queue-stamps-5352', '.claude/worktrees/runtime-factor-stamps-5353', ('src/diagnostics/event_trace.cpp', 'src/policies/default_policy.cpp')),
    ('kissat', 'tools/solvers/variants/kissat-queue-stamps-5352', 'tools/solvers/variants/kissat-factor-stamps-5353', ('src/eventtrace.c', 'src/factor.c'))):
    patch = ''
    for rel in files:
        r = subprocess.run(['diff', '-u', '--label', 'a/' + rel, '--label', 'b/' + rel,
                            root / before / rel, root / after / rel], text=True, capture_output=True)
        assert r.returncode == 1
        patch += r.stdout
    (new / (arm + '-factor-stamps.patch')).write_text(patch)
m = json.loads((old / 'manifest.json').read_text()); m.pop('job', None)
m.update(purpose='UNFUZZED read-only Factor housekeeping snapshots, including empty fresh-variable batches.',
    hypothesis='Native Factor unconditionally restamps even on zero yield; Tactix publishes and consumes advice only after actual factoring.',
    acceptance='Zero stops before BVE 95799, clean scratch; schedule byte-identical 5351/5352. Inspect fresh count and queue stamps around every executed Factor.',
    parent_campaign=str(old.relative_to(root)), source_note='Fresh 5352 copies plus Factor pre/post observer seams; no semantic change.')
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
    for p in [*(new / 'pins').iterdir(), *new.glob('*.patch'), new / 'run.py', new / 'array.sbatch']}
(new / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (new / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
