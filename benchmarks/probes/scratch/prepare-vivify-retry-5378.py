#!/usr/bin/env python3
"""Freeze the isolated large-learned Vivify retry repair and its BVE replay."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5377-retry-producers'
new = root / 'benchmarks/runs/20260907-5378-vivify-retry'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
shutil.copy2(root / 'build/pin-5378-vivify-retry/tactix', new / 'pins/tactix')
shutil.copy2(old / 'pins/kissat', new / 'pins/kissat')
for p in old.glob('*.patch'):
    shutil.copy2(p, new / p.name)
patch = b''
for file in ('src/rules/vivify/vivify.cpp', 'tests/test_vivify.cpp'):
    r = subprocess.run(['diff', '-u', '--label', 'a/' + file, '--label', 'b/' + file,
                        str(root / '.claude/worktrees/runtime-retry-producers-5377' / file),
                        str(root / '.claude/worktrees/runtime-vivify-retry-5378' / file)], capture_output=True)
    assert r.returncode == 1
    patch += r.stdout
(new / 'vivify-retry.patch').write_bytes(patch)
source = (old / 'run.py').read_text()
# Do not invoke the intentional schedule exit. Replay every BVE attempt and
# the actual old decision cutoff to test downstream impact of the repair.
source = '\n'.join(l for l in source.splitlines() if '_BVE_SCHEDULE_' not in l) + '\n'
source = source.replace('env["SAT_BVE_CANDIDATE_EXTERNAL"] = "10700"', 'env["SAT_BVE_CANDIDATE_CONFLICT"] = "343785"')
(new / 'run.py').write_text(source)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)))
m = json.loads((old / 'manifest.json').read_text())
m.update(purpose='Test path-specific learned large Vivify retry repair against the actual next BVE/decision frontier.',
         hypothesis='Rearming retained variables in learned large irredundant Vivify rewrites restores the native initial retry set and BVE trajectory.',
         acceptance='Both clean zero stops at 345034. Compare native bytes against 5374 and Tactix/native BVE begin/end candidates, outcomes/ticks, decision stream and selected state.',
         parent_campaign=str(old.relative_to(root)),
         source_note='Isolated two-file semantic patch, existing primitive only. Learned large irredundant rewrites rearm surviving old literals; large instantiation remains selective. UNFUZZED/unpromoted.')
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(new / 'pins').iterdir(), *new.glob('*.patch'), new / 'run.py', new / 'array.sbatch']}
(new / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (new / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
