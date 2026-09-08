#!/usr/bin/env python3
"""Compare phase-entry candidate prefixes without assuming trajectory success."""
import hashlib
import itertools
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root/'benchmarks/runs/20260907-5433-rank35-inprocess-prefix'
parent = root/'benchmarks/runs/20260907-5424-rank35-rephase-prefix'
m = json.loads((c/'manifest.json').read_text())
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''):h.update(block)
    return h.hexdigest()
def rows(p):
    with p.open() as f:
        for line in f:
            if line.strip() and not line.startswith('#'):yield line.rstrip('\n').split('\t')
for rel,digest in m['hashes'].items():assert sha(c/rel)==digest
if len(list((c/'results').glob('*.json')))!=2:
    print('5433 pending');raise SystemExit(0)
prefixes = {}
for arm in ('candidate','kissat'):
    r = json.loads((c/'results'/(arm+'.json')).read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    for name,digest in r['trace_sha256'].items():assert sha(c/'traces'/name)==digest
    prefix = next(rows(c/'traces'/(arm+'.prefix.tsv')))[1:]
    assert int(prefix[0])==100000
    prefixes[arm]=prefix
first=None;equal=0
for t,k in itertools.zip_longest(rows(c/'traces/candidate.decisions.tsv'),rows(c/'traces/kissat.decisions.tsv')):
    if t!=k:
        first=dict(candidate=t,kissat=k,matching_window_rows=equal);break
    equal+=1
prior = {}
for arm in ('candidate','kissat'):
    p=parent/'traces'/(arm+'.prefix.tsv')
    if (parent/'results'/(arm+'.json')).exists():prior[arm]=next(rows(p))[1:]
if 'kissat' in prior:assert prefixes['kissat']==prior['kissat']
result=dict(clean_stops=2,complete_prefix_identical=prefixes['candidate']==prefixes['kissat'],
    prior_13833_prefix_preserved=prefixes['candidate'][5:8]==['18516218','1','a6eb1df4f4e0e591'],
    prefixes=prefixes,prior_prefixes=prior,first_window_difference=first,matching_window_rows=equal,
    limitation='Stopped diagnostic only. A changed earlier prefix needs an earlier stream, not inference from the current window; no runtime or checked-solve claim.')
p=c/'summary.json'
if p.exists():assert json.loads(p.read_text())==result
else:p.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
