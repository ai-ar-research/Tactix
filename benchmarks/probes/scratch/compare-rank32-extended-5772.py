#!/usr/bin/env python3
"""Validate prior100K digest and all recorded suffix rows of the extension."""
import hashlib
import itertools
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root / 'benchmarks/runs/20260908-5771-reuse-target-rank32-extended'
if (c / 'summary.json').exists():
    print('Already sealed')
    raise SystemExit(0)
if not all((c / 'results' / (a+'.json')).exists() for a in ('candidate','kissat')):
    print('5771 bounded solves pending')
    raise SystemExit(0)
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''):h.update(block)
    return h.hexdigest()
def rows(p):
    with p.open() as stream:
        for line in stream:
            if line.strip() and not line.startswith('#'):yield line.rstrip('\n').split('\t')
m=json.loads((c / 'manifest.json').read_text())
binding=m['parent_summary']
assert sha(root / binding['path'])==binding['sha256']
parent=json.loads((root / binding['path']).read_text())
prefixes={}
for rel,digest in m['hashes'].items():assert sha(c / rel)==digest
for arm in ('candidate','kissat'):
    r=json.loads((c / 'results' / (arm+'.json')).read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    assert r['binary_sha256']==sha(c / 'pins' / arm)
    for rel,digest in r['trace_sha256'].items():assert sha(c / 'traces' / rel)==digest
    p=next(rows(c / 'traces' / (arm+'.prefix.tsv')))[1:]
    assert int(p[0])==m['stop_conflict']
    assert int(p[5])==int(p[6])==40102204
    assert p[7]==parent['prefixes'][arm][4], 'earlier common prefix changed'
    prefixes[arm]=p
count=0;first=None
for a,b in itertools.zip_longest(rows(c / 'traces/candidate.decisions.tsv'),rows(c / 'traces/kissat.decisions.tsv')):
    for row in (a,b):
        if row is not None:assert len(row)==5 and int(row[0])>=100000
    if a!=b:
        first=dict(candidate=a,kissat=b,matching_suffix_rows=count)
        break
    count+=1
same=prefixes['candidate']==prefixes['kissat']
assert (first is None)==same
out=dict(clean_stops=2,stop_conflict=m['stop_conflict'],stream_from=100000,
    earlier_common_rows=40102204,earlier_common_literal_digest_equal=True,
    matching_suffix_rows=count,first_decision_difference=first,complete_prefix_identical=same,prefixes=prefixes,
    limitation='Suffix count starts at100K,not from zero. Diagnostic extension only;frozen6810.066s runtime denominator unchanged.')
(c / 'summary.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))
