#!/usr/bin/env python3
"""Read-only by default; retain changed work without mistaking it for failure."""
import csv, datetime, json, sys
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c=root/'benchmarks/runs/20260909-6584-material-schedule-BC-gate'
hashes={}
def bind(p,expected=None):
    digest=sha(p)
    if expected is not None: assert digest==expected,p
    hashes[str(p.relative_to(root))]=digest
def read(p):
    bind(p); return json.loads(p.read_text())
m=read(c/'manifest.json'); launch=read(c/'launch.json')
assert launch['manifest_sha256']==sha(c/'manifest.json')
for rel,digest in m['pin_sha256'].items(): bind(c/rel,digest)
matrix=list(csv.DictReader((c/'matrix.tsv').open(),delimiter='\t'))
assert len(matrix)==66
records={}; checked=0
for row in matrix:
    task=int(row['task']); path=c/'results'/f'{task:03d}.json'; final=path.exists()
    if not final: path=c/'provisional'/f'{task:03d}.json'
    if not path.exists(): continue
    r=read(path)
    for field in ('arm','instance','expected'): assert r[field]==row[field],(task,field)
    assert int(r['task'])==task and int(r['rank'])==int(row['rank'])
    status=classify(r,final)
    assert status['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(task,status)
    if final:
        assert status['checked'] and status['clean']
        assert r['binary_sha256']==m['arms'][row['arm']]['sha256']
        checked+=1
    records[task]=r
pairs=[]
def work(r):
    lines=r['solver_statistics']
    assert isinstance(lines,list) and lines and all(isinstance(x,str) for x in lines)
    return [x for x in lines if not x.startswith('c timing ')]
assert work(dict(solver_statistics=['c conflicts 1','c timing 2']))==['c conflicts 1']
assert work(dict(solver_statistics=['c conflicts 2']))!=work(dict(solver_statistics=['c conflicts 1']))
for task in range(0,66,2):
    if task not in records or task+1 not in records: continue
    a,b=records[task],records[task+1]
    assert a['instance']==b['instance'] and a['expected']==b['expected']
    pairs.append(dict(instance=a['instance'],rank=a['rank'],expected=a['expected'],
        current_seconds=a['solve']['wall_seconds'],candidate_seconds=b['solve']['wall_seconds'],
        reported_work_equal=work(a)==work(b),certificate_equal=a['certificate_sha256']==b['certificate_sha256']))
print(f"Published {len(records)}/66, expected pairs {len(pairs)}/33, checked/clean {checked}/66, changed-work pairs {sum(not p['reported_work_equal'] for p in pairs)}")
if sys.argv[1:]==['--seal']:
    assert len(records)==66 and len(pairs)==33
    with (c/'summary-6586.json').open('x') as out:
        json.dump(dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
            hashes=hashes,solver_records=66,pairs=pairs,checked_clean=checked,validation_pending=checked!=66,
            limitation='Full solver outcomes permit provisional continuation, not checked acceptance or performance promotion. Changed work is expected; all partial results retained. One timing wave only.'),out,indent=2)
else: assert len(sys.argv)==1
