#!/usr/bin/env python3
"""Seal exact old prefixes and locate the first later Rank46 discrepancy."""
import itertools
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from certificate_prefix import hash_prefix
from decision_prefix import read_prefix, validate_common, validate_stream_accounting
c = root/'benchmarks/runs/20260909-6673-rank46-long-prefix'
target = c/'summary-6675.json'; assert not target.exists()
m = json.loads((c/'manifest.json').read_text()); hashes = {}
def bind(p, expected=None):
    digest = sha(p); assert expected is None or digest == expected,p
    hashes[str(p.relative_to(root))] = digest
for rel,digest in m['hashes'].items(): bind(c/rel,digest)
for rel,digest in m['dependencies'].items(): bind(root/rel,digest)
for name in ('manifest.json','launch.json'): bind(c/name)
old = root/m['cached_controls']; prefixes = {}; streams = {}
for arm in ('candidate','kissat'):
    p = c/f'results/{arm}.json'; bind(p); r = json.loads(p.read_text())
    assert r['arm'] == arm and r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == m['hashes']['pins/'+arm]
    assert r['prior_certificate_prefix'] == m['controls'][arm]['certificate']
    for name,digest in r['trace_sha256'].items(): bind(c/'traces'/name,digest)
    prefix = read_prefix(c/f'traces/{arm}.prefix.tsv')
    control = m['controls'][arm]['prefix']
    validate_common(prefix,stop=1000000,common=control['count'],digest=control['hash'])
    before = old/f'traces/{arm}.decisions.tsv'; after = c/f'traces/{arm}.decisions.tsv'
    assert hash_prefix(after,before.stat().st_size)['sha256'] == sha(before)
    first = last = None; count = 0
    with after.open() as stream:
        for line in stream:
            assert line.endswith('\n')
            row = list(map(int,line.split('\t'))); assert len(row) == 5
            assert 0 <= row[0] <= 1000000 and row[2] > 0 and row[3] != 0 and row[4] in (0,1)
            if first is None: first = row
            last = row; count += 1
    validate_stream_accounting(prefix,first,last,count)
    prefixes[arm] = prefix; streams[arm] = after
differences = 0; first_later = None; samples = []; tail = None
with streams['candidate'].open() as a, streams['kissat'].open() as b:
    for index,(left,right) in enumerate(itertools.zip_longest(a,b)):
        if left == right: continue
        row = dict(index=index,candidate=list(map(int,left.split('\t'))) if left else None,
                   kissat=list(map(int,right.split('\t'))) if right else None)
        differences += 1; tail = row
        if len(samples) < 20: samples.append(row)
        if index >= m['controls']['candidate']['prefix']['count'] and first_later is None: first_later = row
assert first_later and [r['index'] for r in samples[:2]] == [303273,303274]
for p in (Path(__file__),root/'tools/bench/certificate_prefix.py',root/'tools/bench/test_certificate_prefix.py'): bind(p)
out = dict(rank=46,stop=1000000,bounded_stops_clean=True,prior_decisions_and_partial_proofs_exact=True,
    prefixes=prefixes,different_row_count=differences,first_twenty_differences=samples,
    first_difference_after_validated100k=first_later,last_difference=tail,hashes=hashes,
    limitation='All old100K decisions and exact partial-certificate prefixes replay. Known two-row swap remains explicit; the later discrepancy is localization only, not proof of causal origin or runtime benefit. Complete raw streams are hash-bound; samples are not the full discrepancy list.')
with target.open('x') as out_file: json.dump(out,out_file,indent=2); out_file.write('\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('hashes','prefixes','first_twenty_differences')},indent=2))
