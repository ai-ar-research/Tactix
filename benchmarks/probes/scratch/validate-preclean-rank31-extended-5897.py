#!/usr/bin/env python3
"""Validate the common300K prefix and localize the first later portable split."""
import csv
import hashlib
import itertools
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root / 'tools/bench'))
from decision_prefix import read_prefix, validate_common, first_stream_difference
c = root / 'benchmarks/runs/20260908-5893-preclean-rank31-extended'
target = c / 'summary.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8 << 20),b''): h.update(b)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c / rel) == digest
prefixes, boundaries, hashes = {}, {}, {}
fields = ['name','stage','conflicts','decisions','active','unassigned','irred','red','binary']
for arm in ('candidate','kissat'):
    p = c / 'results' / (arm + '.json')
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    assert r['binary_sha256'] == sha(c / 'pins' / arm)
    for rel,digest in r['trace_sha256'].items(): assert sha(c / 'traces' / rel) == digest
    binding = m['cached_controls'][arm]
    old = root / binding['record']
    assert sha(old) == binding['sha256']
    oldrecord = json.loads(old.read_text())
    prior = old.parent.parent / 'traces' / (arm + '.prefix.tsv')
    assert sha(prior) == oldrecord['trace_sha256'][arm + '.prefix.tsv']
    prior_prefix = read_prefix(prior)
    prefix = read_prefix(c / 'traces' / (arm + '.prefix.tsv'))
    assert prior_prefix['count'] == m['common_decisions']
    validate_common(prefix,stop=1000000,common=m['common_decisions'],digest=prior_prefix['hash'])
    prefixes[arm] = prefix
    lines = (c / 'traces' / (arm + '.boundaries.tsv')).read_text().splitlines()
    rows = csv.DictReader(lines[2:],fieldnames=lines[1][2:].split('\t'),delimiter='\t')
    boundaries[arm] = [{key:row[key].lower() if key=='name' else row[key] for key in fields}
                       for row in rows if row['name'].lower() in ('strengthen','bve')]
    hashes[str(p.relative_to(root))] = sha(p)
    hashes[str(old.relative_to(root))] = sha(old)
comparison = first_stream_difference(c / 'traces/candidate.decisions.tsv', c / 'traces/kissat.decisions.tsv')
first_boundary = next((dict(ordinal=i,candidate=a,kissat=b) for i,(a,b) in enumerate(itertools.zip_longest(boundaries['candidate'],boundaries['kissat'])) if a != b),None)
out = dict(prefixes=prefixes,suffix_comparison=comparison,first_forward_bve_difference=first_boundary,hashes=hashes,
    manifest_sha256=sha(c / 'manifest.json'),
    limitation='Cumulative hashes validate cached common300K;complete300K..1M suffix compared until first difference. Not full-state identity,accepted runtime,or a checked final certificate.')
target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(dict(suffix=comparison,first_boundary=first_boundary),indent=2))
