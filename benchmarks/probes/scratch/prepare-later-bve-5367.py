#!/usr/bin/env python3
"""Observe BVE between matched first and unequal second Vivify count images."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3];old=root/'benchmarks/runs/20260907-5366-vivify-counts';new=root/'benchmarks/runs/20260907-5367-later-bve'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for p in (old/'pins').iterdir():shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text();anchor='env[prefix + "_CHOICE_STATE_TRACE"]'
source=source.replace(anchor,'env[prefix + "_BVE_CANDIDATE_TRACE"] = str(C/"traces"/(arm+".bve.tsv"))\nenv["SAT_BVE_CANDIDATE_CONFLICT"] = "132816"\n'+anchor)
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='BVE candidate/outcome history at 132816, between first and second Vivify dose.',hypothesis='The intervening BVE phase changes formula multiplicity or eligibility, creating the unequal second Vivify occurrence counts.',acceptance='Both zero stops with clean scratch; all previous standalone streams unchanged. Compare symmetric BVE begin/end identity, outcome, ticks and limit; retain all records.',parent_campaign=str(old.relative_to(root)),source_note='Exact 5361 pins, existing standalone BVE candidate observer; no code or semantic change.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
