#!/usr/bin/env python3
"""Successor5772: common_reached is a Boolean, not the requested count."""
import hashlib
import itertools
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from decision_prefix import read_prefix, validate_common
c = root / 'benchmarks/runs/20260908-5771-reuse-target-rank32-extended'
assert not (c / 'summary.json').exists(), 'never overwrite sealed results'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
def rows(p):
    with p.open() as stream:
        for line in stream:
            if line.strip() and not line.startswith('#'): yield line.rstrip('\n').split('\t')
m = json.loads((c / 'manifest.json').read_text())
binding = m['parent_summary']
assert sha(root / binding['path']) == binding['sha256']
parent = json.loads((root / binding['path']).read_text())
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
prefixes = {}
for arm in ('candidate', 'kissat'):
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    assert r['binary_sha256'] == sha(c / 'pins' / arm)
    for rel, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / rel) == digest
    p = read_prefix(c / 'traces' / (arm + '.prefix.tsv'))
    assert p['solver'] == ('tactix' if arm == 'candidate' else 'kissat')
    validate_common(p, stop=m['stop_conflict'], common=m['common_decisions'],
                    digest=parent['prefixes'][arm][4])
    prefixes[arm] = p
count = 0
first = None
for a, b in itertools.zip_longest(rows(c / 'traces/candidate.decisions.tsv'),
                                 rows(c / 'traces/kissat.decisions.tsv')):
    for row in (a, b):
        if row is not None: assert len(row) == 5 and int(row[0]) >= 100000
    if a != b:
        first = dict(candidate=a, kissat=b, matching_suffix_rows=count)
        break
    count += 1
same = all(prefixes['candidate'][k] == prefixes['kissat'][k]
           for k in prefixes['candidate'] if k != 'solver')
assert (first is None) == same
out = dict(clean_stops=2, stop_conflict=m['stop_conflict'], stream_from=100000,
           earlier_common_rows=m['common_decisions'], earlier_common_literal_digest_equal=True,
           matching_suffix_rows=count, first_decision_difference=first,
           complete_prefix_identical=same, prefixes=prefixes,
           validator_correction='5772 conflated common count with common_reached Boolean; header-aware parser and four unit tests added. No trace or solver rerun.',
           limitation='Suffix count starts at100K,not zero. Diagnostic only; fixed6810.066s denominator unchanged.')
(c / 'summary.json').write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(out, indent=2))
