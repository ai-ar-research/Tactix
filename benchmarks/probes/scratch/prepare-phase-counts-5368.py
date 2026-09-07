#!/usr/bin/env python3
"""Locate the inprocessing producer of unequal second-Vivify counts."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3];old=root/'benchmarks/runs/20260907-5366-vivify-counts';new=root/'benchmarks/runs/20260907-5368-phase-counts'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for p in (old/'pins').iterdir():shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text();anchor='env[prefix + "_CHOICE_STATE_TRACE"]'
source=source.replace(anchor,'env[prefix + "_INPROCESS_BOUNDARY_TRACE"] = str(C/"traces"/(arm+".boundaries.tsv"))\nenv["SAT_INPROCESS_BOUNDARY_CONFLICT_FROM"] = "132816"\n'+anchor)
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='Outer inprocessing clause-count boundaries at 132816.',hypothesis='Sweep/Substitute/Factor/Congruence or another actual Probe slot creates the unequal second Vivify count image; no ordinary BVE call was observed in 5367.',acceptance='Both zero stops with clean scratch; prior standalone streams unchanged. Compare matched slot begin/end logical clause counts to identify earliest unequal producer, keeping representation-only state separate.',parent_campaign=str(old.relative_to(root)),source_note='Exact 5361 pins, existing standalone slot-boundary observer. Probe source has no ordinary BVE slot; 5367 absence is not a solver failure.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
