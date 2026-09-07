#!/usr/bin/env python3
"""Pin the isolated native-prefix Congruence repair for a stopped replay."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5368-phase-counts'
new = root/'benchmarks/runs/20260907-5369-congruence-prefix'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'): (new/name).mkdir()
shutil.copy2(root/'build/pin-5369-congruence-prefix/tactix', new/'pins/tactix')
shutil.copy2(old/'pins/kissat', new/'pins/kissat')
for p in old.glob('*.patch'): shutil.copy2(p,new/p.name)
before = root/'.claude/worktrees/runtime-queue-bumps-5361'
after = root/'.claude/worktrees/runtime-congruence-prefix-5369'
patch = ''.join(''.join(difflib.unified_diff((before/p).read_text().splitlines(True),
    (after/p).read_text().splitlines(True), fromfile='a/'+p, tofile='b/'+p))
    for p in ('src/rules/congruence/congruence.cpp','tests/test_congruence.cpp'))
(new/'congruence-prefix.patch').write_text(patch)
shutil.copy2(old/'run.py',new/'run.py')
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text()); m.pop('job',None)
m.update(purpose='Isolated Congruence redundant physical-prefix cleanup repair.',
 hypothesis='Native prefix coverage removes the 16 extra redundant clauses at the second Congruence call and closes the selected count/queue/decision frontier.',
 acceptance='Native fixed positive, old negative, repaired positive; full CTests; two zero stopped tasks with clean scratch. Check phase counts and decisions, not wall time. UNFUZZED and unpromoted.',
 parent_campaign=str(old.relative_to(root)),
 source_note='5361 Tactix observer source plus congruence-prefix.patch; same native pin. release_sha256 is ancestral, not this trace binary. Existing logical clause-position helpers only.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest()
 for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
