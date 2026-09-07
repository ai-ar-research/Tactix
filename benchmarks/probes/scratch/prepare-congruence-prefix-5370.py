#!/usr/bin/env python3
"""Extend the repaired decision-prefix replay to 200,000 conflicts."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5369-congruence-prefix'
new=root/'benchmarks/runs/20260907-5370-congruence-prefix-extended'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'): (new/name).mkdir()
for p in (old/'pins').iterdir(): shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'): shutil.copy2(p,new/p.name)
(new/'run.py').write_text((old/'run.py').read_text().replace('"133243"','"200000"'))
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text())
m.update(purpose='Locate the next decision divergence after Congruence prefix repair.',
 hypothesis='The repaired common prefix extends past the old conflict-133243 frontier.',
 acceptance='Two zero stops, clean scratch and unchanged native prefix; identify first portable decision mismatch. Diagnostic, UNFUZZED, no runtime claim.',
 parent_campaign=str(old.relative_to(root)))
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest()
 for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
