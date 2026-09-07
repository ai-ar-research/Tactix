#!/usr/bin/env python3
"""Matched standalone BVE candidate journal at the removal producer phase."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5349-choice-deactivation'
new=root/'benchmarks/runs/20260907-5350-bve-frontier'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'): (new/name).mkdir()
for p in (old/'pins').iterdir(): shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'): shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text()
anchor='env[prefix + "_DEACT_TRACE"]'
assert source.count(anchor)==1
source=source.replace(anchor,'''env[prefix + "_BVE_CANDIDATE_TRACE"] = str(C/"traces"/(arm+".bve.tsv"))
env[prefix + "_INPROCESS_BOUNDARY_TRACE"] = str(C/"traces"/(arm+".boundary.tsv"))
env["SAT_BVE_CANDIDATE_CONFLICT"] = "95799"
env["SAT_INPROCESS_BOUNDARY_CONFLICT_FROM"] = "75000"
'''+anchor)
source=source.replace("'SAT_CHOICE_'","'SAT_CHOICE_', 'SAT_BVE_', 'SAT_INPROCESS_'")
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='UNFUZZED BVE producer localization at conflict 95799 before the first remaining decision split.',
    hypothesis='Diverging BVE candidate order or outcome precedes Tactix-only retirement of 5416. Compare first candidate divergence, not only the final removed variable.',
    acceptance='Both stop zero past 95801 with clean scratch and unchanged decision/state/deactivation files from 5349. Compare candidate external, stage, applied outcome and effort; boundary counters locate phase context.',
    parent_campaign=str(old.relative_to(root)),source_note='Reuse exact immutable 5348/5349 pins; enable existing standalone candidate and inprocessing boundary observers.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
