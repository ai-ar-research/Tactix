#!/usr/bin/env python3
"""Capture selected clause pose/values before and after propagation scans."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3];old=root/'benchmarks/runs/20260907-5364-watch-frontier';new=root/'benchmarks/runs/20260907-5365-watch-pose'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for p in (old/'pins').iterdir():shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text().replace('"132831"','"132816"')
anchor='env[prefix + "_EVENT_TRACE_SKIP_HEAP"] = "1"'
source=source.replace(anchor,anchor+'\nenv[prefix + "_EVENT_TRACE_WATCH_LITERAL"] = "21214"\nenv[prefix + "_EVENT_TRACE_WATCH_POSE"] = "1"\nenv[prefix + "_EVENT_TRACE_WATCH_VALUES"] = "1"\nenv[prefix + "_EVENT_TRACE_WATCH_POST"] = "1"\nenv[prefix + "_EVENT_TRACE_WATCH_ONLY"] = "1"')
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='Selected literal 21214 watch pose/value history across the last inprocessing phase and first propagation split.',hypothesis='Clause -21187,-21186,18559,21214 has different watch pose or replacement cursor before the 132831 scan.',acceptance='Both zero stops and clean scratch; standalone streams unchanged. Compare target clause physical literal order, cursor, blocker and values, retaining duplicate identities and garbage status.',parent_campaign=str(old.relative_to(root)),source_note='Exact 5361 pins. Existing selected-literal watch pre/post pose/value observers; no code or semantic change.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
