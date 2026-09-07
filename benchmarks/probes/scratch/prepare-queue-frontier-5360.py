#!/usr/bin/env python3
"""Reuse exact 5359 pins to observe queue phase boundaries through the split."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5359-focused-choice'
new=root/'benchmarks/runs/20260907-5360-queue-frontier'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for p in (old/'pins').iterdir():shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text()
anchor='env[prefix + "_CHOICE_STATE_TRACE"]'
assert source.count(anchor)==1
source=source.replace(anchor,'env[prefix + "_QUEUE_STAMP_TRACE"] = str(C/"traces"/(arm+".queue.tsv"))\n'+anchor)
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='Read-only full queue boundary history through the focused-choice split; exact 5359 pins.',hypothesis='The relative queue order first differs across a Factor or compaction boundary before conflict 133243.',acceptance='Both stop zero with clean scratch; decision/prefix/selected-state bytes unchanged from 5359. Compare shared-identity queue order and the 21954 neighborhood across boundaries.',parent_campaign=str(old.relative_to(root)),source_note='No new code or build; enable existing standalone queue boundary observer.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
