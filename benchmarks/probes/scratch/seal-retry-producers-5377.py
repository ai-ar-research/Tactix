#!/usr/bin/env python3
"""Check fresh observer neutrality and identify the missing Vivify retry mark."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5377-retry-producers'
old = root / 'benchmarks/runs/20260907-5376-bve-history'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(1048576), b''):
            h.update(b)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, expected in m['hashes'].items():
    assert sha(c / rel) == expected
selected = {}
for i, arm in enumerate(('tactix', 'kissat')):
    log = (c / 'logs' / f'1668919_{i}.log').read_text()
    assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    for suffix in ('decisions.tsv', 'bve.tsv', 'schedule.tsv'):
        assert sha(c / 'traces' / f'{arm}.{suffix}') == sha(old / 'traces' / f'{arm}.{suffix}')
    rows = [dict(x.split('=', 1) for x in l.split('\t')[1:]) for l in (c / 'traces' / f'{arm}.retry.tsv').read_text().splitlines()]
    selected[arm] = [r for r in rows if int(r['conflicts']) > 240040]
assert not selected['tactix'] and len(selected['kissat']) == 1
r = selected['kissat'][0]
assert r['conflicts'] == '331610' and r['source'] == 'vivify_learn_large' and r['before'] == '0'
result = dict(stopped_clean=2, observer_neutral=True, post_last_attempt_marks=selected,
              trace_sha256={p.name: sha(p) for p in (c / 'traces').iterdir()},
              conclusion='Native large learned Vivify rewrite rearms retained variable 10700 at 331610; Tactix has no corresponding mark. Inspect/reproduce that path-specific advice effect without changing occurrence counts.')
p = c / 'summary.json'
if p.exists():
    assert json.loads(p.read_text()) == result
else:
    p.write_text(json.dumps(result, indent=2) + '\n')
print(result['conclusion'])
