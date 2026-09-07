#!/usr/bin/env python3
"""Find first implication/reason divergence after the last common queue phase."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5362-analysis-frontier'
new=root/'benchmarks/runs/20260907-5363-imply-frontier'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for p in (old/'pins').iterdir():shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text().replace('"133184"','"132817"').replace('"133187"','"133186"')
source=source.replace('env[prefix + "_EVENT_TRACE_SKIP_HEAP"] = "1"','env[prefix + "_EVENT_TRACE_SKIP_HEAP"] = "1"\nenv[prefix + "_EVENT_TRACE_IMPLY_ONLY"] = "1"')
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='Implication/reason-only trace after the common 132816 Factor phase through first unequal bump.',hypothesis='Propagation ordering or selected reason diverges before the unequal resolution/bump membership at 133186.',acceptance='Both zero stops with clean scratch; all standalone streams identical 5362; no trace truncation. Compare implication payloads in conflicts 132817--133186.',parent_campaign=str(old.relative_to(root)),source_note='Exact 5361 pins; existing imply-only observer, no code or semantic change.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
