#!/usr/bin/env python3
"""Bind the selected entry/exit clause witness to both complete replay results."""
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
entry = root / 'benchmarks/runs/20260908-6000-bve-clause-entry'
prior = root / 'benchmarks/runs/20260908-5937-first-bve-frontier'
target = entry / 'selected-clause-validation.json'; assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for b in iter(lambda: stream.read(8 << 20), b''): h.update(b)
    return h.hexdigest()
key = '-246596,-5409,-47,49,5415,148944,238853'
rows, hashes = {}, {}
for label, campaign in [('entry', entry), ('exit', prior)]:
    p = campaign / 'summary.json'; summary = json.loads(p.read_text())
    assert summary['observer_parent_trace_and_proof_equal']
    hashes[str(p.relative_to(root))] = sha(p)
    rows[label] = {}
    for arm in ('candidate', 'kissat'):
        p = campaign / 'traces' / (arm + '.formula.tsv')
        assert sha(p) == summary['evidence'][arm]['trace_sha256'][p.name]
        hits = []
        with p.open() as stream:
            for line in stream:
                if key not in line: continue
                fields = line.rstrip().split('\t')
                if fields[3] == key:
                    hits.append(dict(redundant=int(fields[0]), size=int(fields[1]), hash=fields[2],
                                     literals=fields[3], **dict(x.split('=', 1) for x in fields[4:])))
        assert len(hits) == 1, (label, arm, hits)
        rows[label][arm] = hits[0]
for arm in ('candidate', 'kissat'): assert rows['entry'][arm] == rows['exit'][arm]
assert rows['entry']['candidate']['pose'] != rows['entry']['kissat']['pose']
target.write_text(json.dumps(dict(entry_and_exit=rows, hashes=hashes, unchanged_through_forward=True,
    conclusion='The exact clause already has differing pose before firstStrengthen271079. Both arms preserve their own pose, metadata and values through forward. Trace its earlier creation/mutation interval, not forward or heap tie policy.'), indent=2) + '\n')
print('PASS: selected clause differs before forward; each arm preserves its complete row through forward')
