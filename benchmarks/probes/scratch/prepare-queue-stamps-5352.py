#!/usr/bin/env python3
"""Freeze paired queue-stamp lifecycle observers with unchanged BVE endpoint."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5351-bve-schedule'
new = root / 'benchmarks/runs/20260907-5352-queue-stamps'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
for arm, binary in (('tactix', 'build/pin-5352-queue-stamps/tactix'),
                    ('kissat', 'tools/solvers/variants/kissat-queue-stamps-5352/build/kissat')):
    shutil.copy2(root / binary, new / 'pins' / arm)
for p in old.glob('*.patch'):
    shutil.copy2(p, new / p.name)
source = (old / 'run.py').read_text()
anchor = 'env[prefix + "_BVE_SCHEDULE_TRACE"]'
assert source.count(anchor) == 1
source = source.replace(anchor, 'env[prefix + "_QUEUE_STAMP_TRACE"] = str(C/"traces"/(arm+".queue.tsv"))\n' + anchor)
(new / 'run.py').write_text(source)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)))
for arm, before, after, files in (
    ('tactix', '.claude/worktrees/runtime-choice-state-5348', '.claude/worktrees/runtime-queue-stamps-5352', ('src/diagnostics/event_trace.cpp',)),
    ('kissat', 'tools/solvers/variants/kissat-choice-state-5348', 'tools/solvers/variants/kissat-queue-stamps-5352', ('src/eventtrace.c', 'src/compact.c'))):
    patch = ''
    for rel in files:
        r = subprocess.run(['diff', '-u', '--label', 'a/' + rel, '--label', 'b/' + rel,
                            root / before / rel, root / after / rel], text=True, capture_output=True)
        assert r.returncode == 1
        patch += r.stdout
    (new / (arm + '-queue-stamps.patch')).write_text(patch)
m = json.loads((old / 'manifest.json').read_text())
m.pop('job', None)
m.update(purpose='UNFUZZED read-only queue stamp lifecycle at compact boundaries and initial BVE schedule.',
         hypothesis='Numeric relevance differs because queue stamp renumbering or its admission/lifecycle differs.',
         acceptance='Both stop zero before initial BVE at 95799 with clean scratch; schedule files byte-identical 5351. Compare queue identity/order, stamps, mode and search cursor.',
         parent_campaign=str(old.relative_to(root)),
         source_note='Fresh 5348 sources plus queue snapshot observers only; native true queue-compact-pre is before restamping, existing compact-pre is after queue mapping.')
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(new / 'pins').iterdir(), *new.glob('*.patch'), new / 'run.py', new / 'array.sbatch']}
(new / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
