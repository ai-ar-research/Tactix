#!/usr/bin/env python3
"""Observe the count-vector inputs to the first unequal Vivify schedule."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3];old=root/'benchmarks/runs/20260907-5361-queue-bumps';new=root/'benchmarks/runs/20260907-5366-vivify-counts'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for p in (old/'pins').iterdir():shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text();anchor='env[prefix + "_CHOICE_STATE_TRACE"]'
source=source.replace(anchor,'env[prefix + "_VIVIFY_COUNTS_TRACE"] = str(C/"traces"/(arm+".counts.tsv"))\nenv["SAT_VIVIFY_COUNTS_TRACE_CONFLICT"] = "132816"\nenv["SAT_VIVIFY_COUNTS_TRACE_LITERALS"] = "-50,-1334,-2116,-2365,50,1334,2116,2365,-21187,-21186,18559,21214"\n'+anchor)
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='Vivify count-vector inputs at the first unequal second-pass candidate schedule.',hypothesis='Different occurrence counts reverse the relative order of clauses -2116,-1334,-50 and -2365,-1334,-50 in second Vivify tier-one pass at 132816.',acceptance='Both zero stops and clean scratch; standalone streams unchanged from 5361. Inspect existing selected-count observer at each scheduling scope, with exact variable mapping.',parent_campaign=str(old.relative_to(root)),source_note='Exact 5361 pins, existing standalone Vivify count observer, no code or semantic edit.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
