#!/usr/bin/env python3
"""Inspect the first unequal implication at conflict 132832 with existing pins."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3];old=root/'benchmarks/runs/20260907-5362-analysis-frontier';new=root/'benchmarks/runs/20260907-5364-watch-frontier'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for p in (old/'pins').iterdir():shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text().replace('"133184"','"132831"').replace('"133187"','"132832"')
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='Watch/implication detail at first propagation difference after last common queue phase.',hypothesis='The watch stream or clause pose assigns 21214 at different points after common implication 21187 at conflict 132832.',acceptance='Both zero stops with clean scratch; standalone streams byte-identical 5362. Compare bounded watch/reason processing at first 5363 implication mismatch; no trace truncation.',parent_campaign=str(old.relative_to(root)),source_note='Exact 5361 pins, existing bounded full observer, window 132831--132832; no semantic change.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
