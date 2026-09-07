#!/usr/bin/env python3
"""One-change ablation: retain canonical Substitute's per-round retirement order."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5342-deact-trace'
new = root/'benchmarks/runs/20260907-5343-round-retirement'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new/name).mkdir()
shutil.copy2(root/'build/pin-5343-round-retirement/tactix', new/'pins/tactix')
shutil.copy2(old/'pins/kissat', new/'pins/kissat')
for p in old.glob('*.patch'):
    shutil.copy2(p, new/p.name)
shutil.copy2(old/'run.py', new/'run.py')
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(new)))
rel = 'src/policies/default_policy.cpp'
result = subprocess.run(['diff', '-u', '--label', 'a/'+rel, '--label', 'b/'+rel,
    root/'.claude/worktrees/runtime-deact-trace-5342'/rel,
    root/'.claude/worktrees/runtime-round-retirement-5343'/rel], text=True, capture_output=True)
assert result.returncode == 1
(new/'round-retirement.patch').write_text(result.stdout)
m = json.loads((old/'manifest.json').read_text())
m.pop('job', None)
m.update(purpose='UNFUZZED policy ablation: do not globally sort canonical Substitute retirement rounds after Sweep.',
         hypothesis='The obsolete cross-round sort moves three Rank-34 variables before their native round, changes zero-score heap pose and causes the first warm-up decision split.',
         acceptance='Both stop clean without trace truncation at 75452; compare all inheap deactivations and the complete warm-up decision/implication stream. This is neither a proof nor runtime gate.',
         parent_campaign=str(old.relative_to(root)),
         source_note='Exact 5342 diagnostic Tactix source plus round-retirement.patch; reference pin unchanged. Main source unchanged.')
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(new/'pins').iterdir(), *new.glob('*.patch'), new/'run.py', new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
