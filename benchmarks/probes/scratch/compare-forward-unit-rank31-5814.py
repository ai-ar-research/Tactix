#!/usr/bin/env python3
"""Bind repaired full traces and localize their first effects versus parent/native."""
import csv
import hashlib
import itertools
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root / 'tools/bench'))
from decision_prefix import first_stream_difference, read_prefix
c = root / 'benchmarks/runs/20260908-5810-forward-unit-rank31-prefix'
target = c / 'summary.json'
assert not target.exists()
def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''): h.update(block)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
locations = {'repaired':('candidate',c / 'results/candidate.json')}
for label,binding in m['cached_controls'].items():
    path = root / binding['record']
    assert sha(path) == binding['sha256']
    locations['parent' if label == 'candidate' else label] = (label,path)
prefixes, boundaries, streams, evidence = {},{},{},{}
fields = ['name','stage','conflicts','decisions','active','unassigned','irred','red','binary']
for label,(arm,path) in locations.items():
    folder = path.parent.parent
    manifest = json.loads((folder / 'manifest.json').read_text())
    for key in ('input','input_sha256_compressed','stop_conflict','stream_from','common_decisions'):
        assert m[key] == manifest[key], (label,key)
    for rel,digest in manifest['hashes'].items(): assert sha(folder / rel) == digest
    r = json.loads(path.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    assert r['binary_sha256'] == sha(folder / 'pins' / arm)
    for rel,digest in r['trace_sha256'].items(): assert sha(folder / 'traces' / rel) == digest
    prefixes[label] = read_prefix(folder / 'traces' / (arm + '.prefix.tsv'))
    assert prefixes[label]['conflict'] == m['stop_conflict']
    streams[label] = folder / 'traces' / (arm + '.decisions.tsv')
    lines = (folder / 'traces' / (arm + '.boundaries.tsv')).read_text().splitlines()
    assert lines[0] in ('# tactix-inprocess-boundary\t1','# kissat-inprocess-boundary\t1')
    rows = csv.DictReader(lines[2:],fieldnames=lines[1][2:].split('\t'),delimiter='\t')
    boundaries[label] = [{key:row[key].lower() if key == 'name' else row[key] for key in fields}
                         for row in rows if row['name'].lower() in ('strengthen','bve')]
    evidence[label] = dict(result=str(path.relative_to(root)),sha256=sha(path),trace_sha256=r['trace_sha256'])
comparisons = {}
for other in ('parent','kissat'):
    difference = first_stream_difference(streams['repaired'],streams[other])
    same_prefix = all(prefixes['repaired'][key] == prefixes[other][key]
                      for key in prefixes['repaired'] if key != 'solver')
    assert difference['complete_equal'] == same_prefix
    if same_prefix: assert difference['matching_rows'] == prefixes['repaired']['count']
    pairs = list(itertools.zip_longest(boundaries['repaired'],boundaries[other]))
    first = next((dict(ordinal=i,repaired=a,control=b) for i,(a,b) in enumerate(pairs) if a != b),None)
    comparisons[other] = dict(decisions=difference,first_forward_bve_difference=first)
    print(other,json.dumps(comparisons[other]),flush=True)
out = dict(evidence=evidence,prefixes=prefixes,comparisons=comparisons,portable_fields=fields,
    target_boundaries={label:[row for row in rows if row['conflicts'] == '179823'] for label,rows in boundaries.items()},
    manifest_sha256=sha(c / 'manifest.json'),
    limitation='Complete decision-stream comparison and portable forward/BVE counts,not complete physical state identity or runtime causality. Partial proof hashes are provenance,not independently verified certificates. No native rerun.')
target.write_text(json.dumps(out,indent=2)+'\n')
