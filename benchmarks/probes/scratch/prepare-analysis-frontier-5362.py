#!/usr/bin/env python3
"""Reuse 5361 pins for bounded conflict/analysis detail at first unequal bump."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5361-queue-bumps'
new=root/'benchmarks/runs/20260907-5362-analysis-frontier'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for p in (old/'pins').iterdir():shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text();anchor='env[prefix + "_CHOICE_STATE_TRACE"]'
source=source.replace(anchor,'env[prefix + "_EVENT_TRACE"] = str(C/"traces"/(arm+".events.tsv"))\nenv[prefix + "_EVENT_TRACE_CONFLICT_FROM"] = "133184"\nenv[prefix + "_EVENT_TRACE_CONFLICT_TO"] = "133187"\nenv[prefix + "_EVENT_TRACE_LIMIT"] = "2000000"\nenv[prefix + "_EVENT_TRACE_SKIP_HEAP"] = "1"\n'+anchor)
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='Bounded conflict/analysis detail at first unequal active queue bump, conflicts 133184--133187.',hypothesis='Different analyzed-variable membership originates in conflict, reason traversal, or minimization before queue updates at 133186.',acceptance='Both zero stops with clean scratch; decision/prefix/state/queue/bump streams identical 5361. Compare first conflict/learned/analyzed divergence in bounded events; no trace truncation.',parent_campaign=str(old.relative_to(root)),source_note='Exact 5361 observer pins. Enable existing bounded full event trace; suppress heap payloads only.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
