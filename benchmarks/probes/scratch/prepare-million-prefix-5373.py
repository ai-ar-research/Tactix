#!/usr/bin/env python3
"""Extend the exact repaired pins with lightweight decision/boundary streams."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5370-congruence-prefix-extended'
new=root/'benchmarks/runs/20260907-5373-million-prefix'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'): (new/name).mkdir()
for p in (old/'pins').iterdir(): shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'): shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text()
source='\n'.join(l for l in source.splitlines() if not any(s in l for s in ('env[prefix + "_QUEUE','env["SAT_QUEUE','env[prefix + "_VIVIFY','env["SAT_VIVIFY','env[prefix + "_CHOICE','env["SAT_CHOICE')))+'\n'
source=source.replace('"200000"','"1000000"')
source=source.replace("'SAT_BVE_', 'SAT_INPROCESS_'","'SAT_BVE_', 'SAT_INPROCESS_', 'SAT_QUEUE_', 'SAT_VIVIFY_'")
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text())
m.update(purpose='Find the next consequential trajectory split beyond the repaired 200000-conflict frontier.',
 hypothesis='The next decision difference is downstream of a later inprocessing/search transition, not the repaired prefix cleanup.',
 acceptance='Both zero stopped exits, clean scratch, first 475997 decisions identical to 5370 in each arm; retain the next first decision mismatch and preceding slot boundaries. No timing or verdict claim.',
 parent_campaign=str(old.relative_to(root)),source_note='Exact 5369/5370 binaries; only lightweight standalone decision and rule-boundary observers enabled. UNFUZZED/unpromoted.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest()
 for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
