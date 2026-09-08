#!/usr/bin/env python3
"""Check native verbose transitions and every predecessor prefix row."""
import hashlib
import json
import re
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5458-rank37-native-bound'
old=root/'benchmarks/runs/20260907-5453-rank37-eliminate-limit-prefix'
p=c/'results/kissat.json'
if not p.exists():print('5458 pending');raise SystemExit(0)
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''):h.update(block)
    return h.hexdigest()
def rows(p):
    with p.open() as f:
        for line in f:
            if line.strip() and not line.startswith('#'):yield line.rstrip('\n').split('\t')
m=json.loads((c/'manifest.json').read_text());r=json.loads(p.read_text())
assert r['exit_code']==0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
for rel,digest in m['hashes'].items():assert sha(c/rel)==digest
for rel,digest in r['trace_sha256'].items():assert sha(c/'traces'/rel)==digest
prior=json.loads((old/'results/kissat.json').read_text())
before=old/'traces/kissat.decisions.tsv'
assert sha(before)==prior['trace_sha256'][before.name]
stream=rows(before);count=0
for row in rows(c/'traces/kissat.decisions.tsv'):
    assert int(row[0])<m['stop_conflict'] and next(stream)==row
    count+=1
assert int(next(stream)[0])>=m['stop_conflict']
log=(c/'logs/kissat.out').read_text()
transitions=[list(map(int,v)) for v in re.findall(r'completed elimination bound (\d+) next (\d+)',log)]
cuts=[line for line in log.splitlines() if 'resolution limit' in line]
assert transitions[:2]==[[0,1],[1,2]],transitions
out=dict(clean_stop=True,observer_neutral_rows=count,transitions=transitions,
    resolution_limit_lines=cuts,log_sha256=sha(c/'logs/kissat.out'),
    conclusion='Native consistent-phase bound advances 0->1->2 before conflict 32073; full predecessor stream preserved through stop.',
    limitation='Controlled-stop source confirmation; no complete proof or performance claim.')
target=c/'summary.json'
if target.exists():assert json.loads(target.read_text())==out
else:target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))
