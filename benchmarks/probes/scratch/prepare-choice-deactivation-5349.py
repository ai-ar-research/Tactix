#!/usr/bin/env python3
"""Reuse the selected-state pins and expose their existing deactivation observer."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5348-choice-state'
new=root/'benchmarks/runs/20260907-5349-choice-deactivation'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'): (new/name).mkdir()
for p in (old/'pins').iterdir(): shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'): shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text()
anchor='env.update({prefix + "_CHOICE_STATE_TRACE"'
assert source.count(anchor)==1
source=source.replace(anchor,'env[prefix + "_DEACT_TRACE"] = str(C/"traces"/(arm+".deact.tsv"))\n'+anchor)
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='UNFUZZED removal/liveness producer for 5416 absent from Tactix heap but unassigned at conflict 95801.',
    hypothesis='An earlier rule deactivated 5416 in Tactix only; compare all inheap removals and selected-variable records.',
    acceptance='Both exit zero at the same prefix endpoint and scratch clean; full decision and selected-state outputs byte-identical to 5348.',
    parent_campaign=str(old.relative_to(root)),source_note='Exact immutable 5348 pins; enable existing standalone deactivation logging, no source change.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
