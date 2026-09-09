#!/usr/bin/env python3
"""Seal the complete first100K streams, retaining all two differing rows."""
import itertools
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_stream_accounting, first_stream_difference
c = root/'benchmarks/runs/20260909-6669-rank46-current-prefix'
target = c/'summary-6671.json'; assert not target.exists()
m = json.loads((c/'manifest.json').read_text()); hashes = {}
def bind(p, expected=None):
    digest = sha(p); assert expected is None or digest == expected,p
    hashes[str(p.relative_to(root))] = digest
for rel,digest in m['hashes'].items(): bind(c/rel,digest)
for rel,digest in m['dependencies'].items(): bind(root/rel,digest)
for name in ('manifest.json','launch.json'): bind(c/name)
prefixes = {}; rows = {}
for arm in ('candidate','kissat'):
    p = c/f'results/{arm}.json'; bind(p); r = json.loads(p.read_text())
    assert r['arm'] == arm and r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == m['hashes']['pins/'+arm]
    for name,digest in r['trace_sha256'].items(): bind(c/'traces'/name,digest)
    p = read_prefix(c/f'traces/{arm}.prefix.tsv'); assert p['conflict'] == 100000
    content = (c/f'traces/{arm}.decisions.tsv').read_text(); assert content.endswith('\n')
    rows[arm] = [list(map(int,line.split('\t'))) for line in content.splitlines()]
    assert all(len(row) == 5 and 0 <= row[0] <= 100000 and row[2] > 0 and row[3] != 0 and row[4] in (0,1) for row in rows[arm])
    validate_stream_accounting(p,rows[arm][0],rows[arm][-1],len(rows[arm]))
    assert p['count'] == 362439 and p['end_decisions'] == 355748
    prefixes[arm] = p
differences = [dict(index=i,candidate=a,kissat=b) for i,(a,b) in enumerate(itertools.zip_longest(rows['candidate'],rows['kissat'])) if a != b]
assert len(differences) == 2
assert [r['index'] for r in differences] == [303273,303274]
assert [[r[a][0],r[a][1],r[a][2],r[a][4]] for r in differences for a in ('candidate','kissat')] == [[78797,296583,63,0]]*2 + [[78797,296584,64,0]]*2
assert [r['candidate'][3] for r in differences] == [-18626,-18594]
assert [r['kissat'][3] for r in differences] == [-18594,-18626]
comparison = first_stream_difference(c/'traces/candidate.decisions.tsv',c/'traces/kissat.decisions.tsv')
for p in (Path(__file__),root/'tools/bench/decision_prefix.py'): bind(p)
out = dict(rank=46,stop=100000,bounded_stops_clean=True,prefixes=prefixes,first_difference=comparison,
    all_differences=differences,hashes=hashes,
    conclusion='Only two adjacent focused decision literals swap at78797/296583-296584; every other row through100K agrees, including all following counters/levels/literals. This is not evidence that the swap causes the eventual1.823x conflict gap. Extend the bound to locate a sustained later divergence before choosing a repair.',
    limitation='Partial proofs hashed and removed, not final UNSAT certificates. Diagnostic observer neutrality requires exact own replay in a successor. No source change, runtime refresh, full-formula equality or search-gap causality claim.')
with target.open('x') as out_file: json.dump(out,out_file,indent=2); out_file.write('\n')
print(out['conclusion'])
