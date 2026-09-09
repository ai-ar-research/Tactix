#!/usr/bin/env python3
"""Compare repaired full100K stream with cached native without assumed closure."""
import csv
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix,validate_stream_accounting,first_stream_difference
from transitive_trace import binary_drops
c=root/'benchmarks/runs/20260909-6433-activation-rank45-prefix'; target=c/'summary.json'
assert not target.exists()
p=c/'results/candidate.json'
if not p.exists(): print('Completed candidate record required; no output created'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text()); r=json.loads(p.read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest
for b in [*m['authorities'].values(),m['parent_summary'],m['gate'],m['schedule_control'],m['cached_native']]: assert sha(root/b['path'])==b['sha256']
assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
assert r['binary_sha256']==sha(c/'pins/candidate') and r['partial_certificate']['bytes']>0
hashes={str((c/n).relative_to(root)):sha(c/n) for n in ('manifest.json','launch.json','results/candidate.json')}
for name,digest in r['trace_sha256'].items():
    p=c/'traces'/name; assert sha(p)==digest; hashes[str(p.relative_to(root))]=digest
prefix=read_prefix(c/'traces/candidate.prefix.tsv'); assert prefix['conflict']==100000 and prefix['common_reached']==1 and prefix['common']==1
stream=c/'traces/candidate.decisions.tsv'
count=int(subprocess.check_output(['wc','-l',str(stream)],text=True).split()[0])
first=list(map(int,subprocess.check_output(['head','-n','1',str(stream)],text=True).split()))
last=list(map(int,subprocess.check_output(['tail','-n','1',str(stream)],text=True).split()))
validate_stream_accounting(prefix,first,last,count)
old=root/m['cached_native']['path']; oracle=json.loads(old.read_text()); native=old.parents[1]/'traces/kissat.decisions.tsv'
assert sha(native)==oracle['trace_sha256'][native.name]; hashes[str(native.relative_to(root))]=sha(native)
difference=first_stream_difference(stream,native)
lines=(c/'traces/candidate.boundaries.tsv').read_text().splitlines()
rows=list(csv.DictReader(lines[2:],fieldnames=lines[1][2:].split('\t'),delimiter='\t'))
result=dict(rank=45,stop=100000,prefix=prefix,comparison_native=difference,
    transitive76464=binary_drops(rows,'TransitiveReduce',76464),hashes=hashes,
    limitation='Whole bounded selected stream only, not full graph/solve/runtime or final proof acceptance. A first difference is retained evidence, not discarded measurement.')
target.write_text(json.dumps(result,indent=2)+'\n'); print(json.dumps({k:v for k,v in result.items() if k!='hashes'},indent=2))
