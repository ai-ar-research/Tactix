#!/usr/bin/env python3
"""Matched decision-structure removal stream, with the full warm-up observer."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5341-warmup-reference'
new = root/'benchmarks/runs/20260907-5342-deact-trace'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new/name).mkdir()
for arm, source in (
    ('tactix', root/'build/pin-5342-deact-trace/tactix'),
    ('kissat', root/'tools/solvers/variants/kissat-deact-trace-5342/build/kissat')):
    shutil.copy2(source, new/'pins'/arm)
for p in old.glob('*.patch'):
    shutil.copy2(p, new/p.name)
source = (old/'run.py').read_text()
anchor = 'env.update({prefix + "_EVENT_TRACE":'
assert source.count(anchor) == 1
source = source.replace(anchor, 'env[prefix + "_DEACT_TRACE"] = str(C / "traces" / (arm + ".deact.tsv"))\n'+anchor)
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(new)))
for name, before, after, rel in (
    ('tactix-deact-observer.patch', '.claude/worktrees/runtime-bridge-trace-5327', '.claude/worktrees/runtime-deact-trace-5342', 'src/policies/heuristics.cpp'),
    ('kissat-deact-observer.patch', 'tools/solvers/variants/kissat-warmup-imply-5341', 'tools/solvers/variants/kissat-deact-trace-5342', 'src/flags.c')):
    result = subprocess.run(['diff', '-u', '--label', 'a/'+rel, '--label', 'b/'+rel, root/before/rel, root/after/rel], text=True, capture_output=True)
    assert result.returncode == 1
    (new/name).write_text(result.stdout)
m = json.loads((old/'manifest.json').read_text())
m.pop('job', None)
m.update(purpose='UNFUZZED deactivation-order trace before the Rank-34 zero-score warm-up tie.',
         hypothesis='Same warm-up propagation prefix but different zero-score choice originates in earlier decision-structure removal order.',
         acceptance='Both stopped clean at 75452 with no truncation; existing decision and implication rows unchanged from 5341. Compare deactivated external variables, conflict counters, reason class and heap membership; trail offsets differ by root representation.',
         parent_campaign=str(old.relative_to(root)),
         source_note='Fresh immutable Tactix/Kissat trace builds add standalone deactivation logging only; patches are relative to named predecessor source copies.')
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(new/'pins').iterdir(), *new.glob('*.patch'), new/'run.py', new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
