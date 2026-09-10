#!/usr/bin/env python3
"""Bind selected producer/history additions only after exact schedule replay."""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7024-rank35-retry-producer'
target = c / 'replay-summary-7026.json'
assert not target.exists()
m = json.loads((c / 'manifest.json').read_text()); parent = root / m['parent']
if any(not (c / 'results' / (a + '.json')).exists() for a in m['arms']):
    print('Producer tasks pending; no summary written'); sys.exit(0)
hashes = {}
def bind(p, expected=None):
    actual = sha(p)
    assert expected is None or actual == expected, p
    hashes[str(p.relative_to(root))] = actual
for rel,digest in m['hashes'].items(): bind(c / rel, digest)
for rel,digest in m['dependencies'].items(): bind(root / rel, digest)
for p in (c / 'manifest.json', c / 'launch.json', Path(__file__)): bind(p)
additions = {}
for arm in m['arms']:
    p = c / 'results' / (arm + '.json'); bind(p)
    r = json.loads(p.read_text())
    old = json.loads((parent / 'results' / p.name).read_text())
    assert r['arm'] == arm and r['exit_code'] == 0 and r['scratch_deleted']
    assert not Path(r['scratch']).exists()
    assert r['binary_sha256'] == old['binary_sha256'] == m['hashes']['pins/' + arm]
    assert r['partial_certificate'] == old['partial_certificate']
    expected_new = {arm + '.retry-producer.tsv', arm + '.bve-history.tsv'}
    assert set(r['trace_sha256']) == set(old['trace_sha256']) | expected_new
    for name, digest in old['trace_sha256'].items():
        assert r['trace_sha256'][name] == digest
        bind(parent / 'traces' / name, digest)
    for name, digest in r['trace_sha256'].items(): bind(c / 'traces' / name, digest)
    additions[arm] = {}
    for name in sorted(expected_new):
        content = (c / 'traces' / name).read_text()
        assert content and content.endswith('\n'), 'missing/truncated selected journal'
        additions[arm][name] = dict(rows=len(content.splitlines()), path=str((c / 'traces' / name).relative_to(root)))
out = dict(hashes=hashes, exact_whole_parent_schedule_trace_and_partial_proof_replay=True,
    selected_external=m['selected_external'], additions=additions,
    limitation='Observer neutrality only. Selected journals are hash-bound and newline-complete, not yet schema-normalized or causally interpreted; validate their fields and candidate lifecycle before attributing a retry difference.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps(additions, indent=2))
