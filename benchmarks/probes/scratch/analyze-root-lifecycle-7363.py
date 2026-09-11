#!/usr/bin/env python3
"""Bind whole replay and assign the missed transitions to the Lucky boundary."""
import gzip
import json
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
def deact(line):
    match=re.fullmatch(r'DEACT ([0-9]+) ([1-9][0-9]*) elim=([01]) fixed=([01]) inheap=([01]) trail=([0-9]+)',line.strip())
    assert match
    return list(map(int,match.groups()))
assert deact('DEACT 0 1333 elim=0 fixed=1 inheap=0 trail=1')==[0,1333,0,1,0,1]
for bad in ('DEACT 0 0 elim=0 fixed=1 inheap=0 trail=1', 'DEACT 0 1333 elim=2 fixed=1 inheap=0 trail=1', 'DEACT 0 1333 elim=0 fixed=1 inheap=0 trail=1 extra'):
    try:deact(bad)
    except AssertionError:pass
    else:raise AssertionError('bad deactivation accepted')
if sys.argv[1:]==['--self-test']:print('deactivation schema: 3 negative controls PASS');sys.exit(0)
assert not sys.argv[1:]
c=root/'benchmarks/runs/20260911-7362-root-lifecycle';old=root/'benchmarks/runs/20260911-7358-root-queue'
target=root/'benchmarks/reports/2026-09-11-root-lifecycle-analysis-7363.json';assert not target.exists()
qpath=root/'benchmarks/reports/2026-09-11-root-queue-analysis-7359.json'
selected=set(json.loads(qpath.read_text())['post_parent_only']);assert len(selected)==29
hashes={};analyses=[]
def bind(p,expected=None):
    h=sha(p);assert expected is None or h==expected,p;hashes[str(p.relative_to(root))]=h
helper=Path(__file__).with_name('check-submit-root-lifecycle-7362.py')
check=subprocess.run([sys.executable,helper],text=True,capture_output=True);assert check.returncode==0,(check.stdout,check.stderr)
for task in range(2):
    path=c/f'results/{task:03d}.json';record=json.loads(path.read_text());assert classify(record,True)['checked'];bind(path)
    prior_path=old/f'results/{task:03d}.json';prior=json.loads(prior_path.read_text());assert classify(prior,True)['checked'];bind(prior_path)
    for kind,artifact in record['trace_artifacts'].items():
        path=c/artifact['path'];bind(path,artifact['sha256'])
        if kind not in ('roots','deact'):
            authority=prior['trace_artifacts'][kind];bind(old/authority['path'],authority['sha256']);bind(path,authority['sha256'])
    with gzip.open(c/f'traces/{task:03d}-roots.tsv.gz','rt') as stream:lines=stream.read().splitlines()
    assert lines[:2]==['# tactix-event-trace\t1','# detail-conflicts\t0\t14492']
    rows=[line.split('\t') for line in lines if not line.startswith('#')]
    assert rows and all(len(row)==22 and row[0]=='tactix' for row in rows)
    assert [int(row[1]) for row in rows]==list(range(len(rows)))
    assert rows[0][2]=='trace_begin' and rows[-1][2]=='terminate' and len(rows)<1000000
    roots=[]
    for row in rows:
        if row[2]!='root_assign':continue
        payload=dict(part.split('=',1) for part in row[-1].split('|'))
        assert set(payload)=={'lit','warming','trail_after','unflushed_after','reason'}
        lit=int(payload['lit']);assert abs(lit) in selected
        assert int(row[3])==int(row[4])==int(row[6])==0 and row[7]=='focused'
        assert payload['warming']=='0' and payload['reason']=='unit'
        roots.append(dict(lit=lit,propagations=int(row[5]),trail_after=int(payload['trail_after']),row=row))
    assert len(roots)==29 and {abs(r['lit']) for r in roots}==selected
    assert [r['trail_after'] for r in roots]==list(range(2870,2899))
    with gzip.open(c/f'traces/{task:03d}-macro.tsv.gz','rt') as stream:macros=[line.rstrip().split('\t') for line in stream if not line.startswith('#')]
    late=[row for row in macros if row[2] in ('lucky_late_begin','lucky_late_end')]
    assert len(late)==2 and int(late[0][5])==315251 and int(late[1][5])==514529
    assert all(int(late[0][5])<r['propagations']<int(late[1][5]) for r in roots)
    with gzip.open(c/f'traces/{task:03d}-deact.tsv.gz','rt') as stream:deactivations=[deact(line) for line in stream]
    matches=[r for r in deactivations if r[1] in selected]
    assert not matches,('selected roots unexpectedly deactivated',matches)
    analyses.append(dict(roots=roots,late_boundaries=late,selected_deactivations=matches,
        all_deactivation_rows=len(deactivations),root_trace_rows=len(rows),checked=True))
for p in (Path(__file__),helper,qpath,c/'manifest.json',c/'launch.json'):bind(p)
assert analyses[0]['roots']==analyses[1]['roots']
with target.open('x') as out:json.dump(dict(status='CHECKED_FULL_REPLAY_LATE_LUCKY_ROOT_LIFECYCLE_MISS',hashes=hashes,
    analyses=analyses,reader_output=check.stdout,
    finding='All29 selected roots are learned during late Lucky atC0 before search, with no deactivation anywhere in either complete solve. Both paths call lucky_apply directly without consuming its newly proved roots. H is attached for late Lucky; deferred early Lucky instead gets its fixed-root image on subsequent H attachment.',
    limitation='Explains this queue/BVE regression; not a runtime or memory-gain claim. Fix belongs to DefaultPolicy lifecycle, not Lucky guard or compaction-stamp offset.'),out,indent=2)
print(target)
