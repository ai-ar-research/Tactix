#!/usr/bin/env python3
"""Compare current complete100K decisions with the cached native control."""
import hashlib
import itertools
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root / 'benchmarks/runs/20260908-5769-reuse-target-rank32-prefix'
if (c / 'summary.json').exists():
    print('Already sealed')
    raise SystemExit(0)
if not (c / 'results/candidate.json').exists():
    print('5769 candidate stop pending')
    raise SystemExit(0)
m=json.loads((c / 'manifest.json').read_text())
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''):h.update(block)
    return h.hexdigest()
def rows(p):
    with p.open() as stream:
        for line in stream:
            if line.strip() and not line.startswith('#'):yield line.rstrip('\n').split('\t')
native=root / m['cached_native_campaign']
binding=m['cached_controls']['kissat']
assert sha(root / binding['record'])==binding['sha256']
prefixes={}
for arm,folder in [('candidate',c),('kissat',native)]:
    fm=json.loads((folder / 'manifest.json').read_text())
    for rel,digest in fm['hashes'].items():assert sha(folder / rel)==digest
    for key in ('input','input_sha256_compressed','stop_conflict','stream_from','common_decisions'):
        assert fm[key]==m[key],key
    r=json.loads((folder / 'results' / (arm+'.json')).read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    assert r['binary_sha256']==sha(folder / 'pins' / arm)
    for rel,digest in r['trace_sha256'].items():assert sha(folder / 'traces' / rel)==digest
    prefixes[arm]=next(rows(folder / 'traces' / (arm+'.prefix.tsv')))[1:]
    assert int(prefixes[arm][0])==m['stop_conflict']
count=0;first=None
for a,b in itertools.zip_longest(rows(c / 'traces/candidate.decisions.tsv'),rows(native / 'traces/kissat.decisions.tsv')):
    if a!=b:
        first=dict(candidate=a,kissat=b,matching_rows=count)
        break
    assert len(a)==5
    count+=1
same=prefixes['candidate']==prefixes['kissat']
assert (first is None)==same,'prefix/stream mismatch'
summary=dict(clean_candidate_stop=True,cached_native_validated=True,matching_rows=count,
    stop_conflict=m['stop_conflict'],complete_prefix_identical=same,first_decision_difference=first,
    prefixes=prefixes,manifest_sha256=sha(c / 'manifest.json'),
    limitation='Current-source stopped prefix,not runtime or complete proof. Native control reused,not rerun. If equal through100K,extend beyond cached coverage for the larger full-run regression.')
(c / 'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
