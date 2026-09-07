#!/usr/bin/env python3
"""Expose BVE outcomes and choice state at the next actual formula split."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5373-million-prefix'
new=root/'benchmarks/runs/20260907-5374-bve-frontier'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'): (new/name).mkdir()
for p in (old/'pins').iterdir(): shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'): shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text().replace('"1000000"','"345034"')
anchor='env.update({prefix + "_DECISION_STREAM_TRACE"'
observer='''env[prefix + "_BVE_CANDIDATE_TRACE"] = str(C/"traces"/(arm+".bve.tsv"))
env["SAT_BVE_CANDIDATE_CONFLICT"] = "343785"
env[prefix + "_CHOICE_STATE_TRACE"] = str(C/"traces"/(arm+".state.tsv"))
env["SAT_CHOICE_STATE_CONFLICT"] = "345034"
env["SAT_CHOICE_STATE_VARIABLE"] = "9959"
'''
assert source.count(anchor)==1
(new/'run.py').write_text(source.replace(anchor,observer+anchor))
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text())
m.update(purpose='Localize BVE formula-state difference preceding the 345034 choice split.',
 hypothesis='The extra elimination at 343785 explains why Tactix cannot or does not choose native variable 9959 at 345034.',
 acceptance='Both zero stops/clean scratch; per-arm decision bytes match 5373 through this cutoff. Compare every BVE candidate/outcome/tick, then selected variable assignment/heap/queue state.',
 parent_campaign=str(old.relative_to(root)),source_note='Exact existing observer pins; no semantic change. UNFUZZED/unpromoted.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
