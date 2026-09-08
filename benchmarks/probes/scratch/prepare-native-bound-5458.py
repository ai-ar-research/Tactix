#!/usr/bin/env python3
"""Read native verbose bound transitions without changing a solver or budget."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5453-rank37-eliminate-limit-prefix'
c=root/'benchmarks/runs/20260907-5458-rank37-native-bound'
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'):(c/name).mkdir()
shutil.copy2(old/'pins/kissat',c/'pins/kissat')
runner=(old/'run.py').read_text().replace("'tactix-5453-'","'tactix-5458-'")
needle="str(c / 'pins' / arm), str(cnf), str(proof)]"
assert runner.count(needle)==1
runner=runner.replace(needle,"str(c / 'pins' / arm), '--verbose=3', str(cnf), str(proof)]")
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('--array=0-1','--array=1').replace('bridge-decision-window','5458-native-bound'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((old/'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),stop_conflict=32074,stream_from=0,
 created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
 hypothesis='Static native eliminate_variables releases the heap before computing remain and shadows last_round_eliminated, making its successful phase bound transition unconditional. Confirm bound 0->1 then 1->2 before Factor at 32073; Tactix remains at 1 after its quota cut.',
 validation='One clean controlled native stop, full unchanged prefix against 5453, verbose native bound/effort transitions. No timing or complete proof claim.',
 contract='Single array index 1, 12 logical CPUs/48000 MiB, no throttle/node constraint, immutable native binary and ordinary verbosity only.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c/'pins/kissat',c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
(c/'pins/kissat').chmod(0o555)
print(c)
