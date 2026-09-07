#!/usr/bin/env python3
"""Compare complete initial BVE heaps and preceding queue restamp images."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5374-bve-frontier'
new=root/'benchmarks/runs/20260907-5375-bve-schedule'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'): (new/name).mkdir()
for p in (old/'pins').iterdir(): shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'): shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text()
anchor='env.update({prefix + "_DECISION_STREAM_TRACE"'
observer='''env[prefix + "_BVE_SCHEDULE_TRACE"] = str(C/"traces"/(arm+".schedule.tsv"))
env["SAT_BVE_SCHEDULE_CONFLICT"] = "343785"
env["SAT_BVE_SCHEDULE_ORDINAL"] = "0"
env["SAT_BVE_SCHEDULE_MEMBERS"] = "1"
env[prefix + "_QUEUE_STAMP_TRACE"] = str(C/"traces"/(arm+".queue.tsv"))
'''
assert source.count(anchor)==1
(new/'run.py').write_text(source.replace(anchor,observer+anchor))
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text())
m.update(purpose='Separate initial BVE priority/topology discrepancy from dynamic update effects.',
 hypothesis='The first 12 candidates match before candidate 13 changes; either initial relative priorities/topology differ, or earlier matched elimination updates differ.',
 acceptance='Both zero stops before first BVE candidate at 343785, clean scratch. Decision stream is a byte prefix of 5374. Compare complete membership, occurrence counts, scores and physical positions, plus prior queue boundaries.',
 parent_campaign=str(old.relative_to(root)),source_note='Exact immutable 5369/5370 pins. Existing schedule observer intentionally exits before BVE; candidate and final decision-prefix files may therefore be absent. UNFUZZED/unpromoted.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
