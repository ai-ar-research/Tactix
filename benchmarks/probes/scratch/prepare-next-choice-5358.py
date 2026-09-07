#!/usr/bin/env python3
"""Selected-variable state at the next ordinary split after the Factor repair."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5355-empty-factor-prefix'
new=root/'benchmarks/runs/20260907-5358-next-choice'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for p in (old/'pins').iterdir():shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text()
start=source.index('env[prefix + "_BVE_CANDIDATE_TRACE"]');end=source.index('env.update({prefix + "_DECISION_STREAM_TRACE"',start)
source=source[:start]+'''env[prefix + "_CHOICE_STATE_TRACE"] = str(C/"traces"/(arm+".state.tsv"))
env["SAT_CHOICE_STATE_CONFLICT"] = "133243"
env["SAT_CHOICE_STATE_VARIABLE"] = "21954"
'''+source[end:]
source=source.replace('"200000"','"133243"')
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='UNFUZZED selected state at new ordinary decision frontier 133243.',
    hypothesis='Variable 21954 has unequal assignment, rank or score state before the new selection mismatch.',
    acceptance='Zero stops beyond 133243 and clean scratch; full scalar decision streams equal the matching 5355 prefix. Inspect state before and at ordinary decision 327447.',
    parent_campaign=str(old.relative_to(root)),source_note='Exact 5354/5353 pins, existing selected-state observer; no new code or semantic change.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
