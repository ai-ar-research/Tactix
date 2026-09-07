#!/usr/bin/env python3
"""Validate unchanged boundaries, then expose exact source images and batches."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
m = json.loads((C/'manifest.json').read_text())
for task, arm in enumerate(('core', 'sequential')):
    r = json.loads((C/'results'/f'{task:03d}.json').read_text())
    assert r['task'] == task and r['arm'] == arm and r['rank'] == 21 and r['valid']
    assert r['solve']['exit_code'] == 10 and not r['solve']['timed_out']
    assert r['model_check']['result'] == 'verified' and r['model_check']['exit_code'] == 0
    assert r['artifact_deleted'] and not r['cleanup_pending']
    assert r['binary_sha256'] == m['arms'][arm]['sha256']
    assert (C/'traces'/f'{arm}.tsv').read_bytes() == (C.parent/'20260907-5333-rank21-standalone'/'traces'/f'{arm}.tsv').read_bytes()
raw = (C/'traces/sequential.lifecycle.tsv').read_bytes()
rows = [l.split('\t') for l in raw.decode().splitlines() if not l.startswith('#')]
targets = [r for r in rows if r[2] == 'sweep-physical-target' and r[4] == '23889']
assert len(targets) == 4
emissions = (C/'traces/sequential.emission.tsv').read_bytes()
eq = [list(map(int, l.split('\t'))) for l in emissions.decode().splitlines()]
selected = {abs(int(v)) for r in targets for v in r[7:9]}
batches = {r[1] for r in eq if abs(r[3]) in selected or abs(r[4]) in selected}
print(json.dumps(dict(checked_clean=2, boundary_traces_identical_to_5333=True,
    targets=targets, selected_batches=[r for r in eq if r[1] in batches],
    lifecycle_sha256=hashlib.sha256(raw).hexdigest(),
    emission_sha256=hashlib.sha256(emissions).hexdigest()), indent=2))
