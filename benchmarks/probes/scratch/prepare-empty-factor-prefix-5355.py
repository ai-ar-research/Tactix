#!/usr/bin/env python3
"""Find the next decision frontier after the empty Factor lifecycle repair."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5354-empty-factor'
new = root / 'benchmarks/runs/20260907-5355-empty-factor-prefix'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'): (new/name).mkdir()
for p in (old/'pins').iterdir(): shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'): shutil.copy2(p,new/p.name)
source = (old/'run.py').read_text()
start=source.index('env[prefix + "_QUEUE_STAMP_TRACE"]'); end=source.index('try:\n',start)
source=source[:start]+'''env[prefix + "_BVE_CANDIDATE_TRACE"] = str(C/"traces"/(arm+".bve.tsv"))
env["SAT_BVE_CANDIDATE_CONFLICT"] = "95799"
env.update({prefix + "_DECISION_STREAM_TRACE": str(C/"traces"/(arm+".decisions.tsv")),
            prefix + "_DECISION_PREFIX_TRACE": str(C/"traces"/(arm+".prefix.tsv")),
            "SAT_DECISION_PREFIX_CONFLICT": "200000",
            "SAT_DECISION_PREFIX_COMMON": "1",
            "SAT_DECISION_PREFIX_STOP": "1"})
'''+source[end:]
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='UNFUZZED cumulative decision frontier after empty Factor repair.',
    hypothesis='Restoring the initial BVE heap removes the 95801 choice split; identify any next divergence rather than assuming full closure.',
    acceptance='Zero stop at the first checkpoint after 200000, clean scratch; recompute whole decision FNV hashes and compare scalar streams and symmetric BVE begin/end records.',
    parent_campaign=str(old.relative_to(root)),source_note='Exact immutable 5354 Tactix and 5353 native observer pins, no source/build mutation.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
