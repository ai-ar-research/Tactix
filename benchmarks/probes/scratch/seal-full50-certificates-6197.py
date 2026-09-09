#!/usr/bin/env python3
"""Successor checkpoint; never rewrite provisional scores or old reports."""
import datetime as dt
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
import fixed_runtime as f
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-09-full50-certificates-6197.json'
assert not target.exists()
campaigns = {}; hashes = {}
for name in ('20260908-6115-substitute-publication-ranks1-50',
             '20260909-6181-learned-sort-ranks1-50', '20260909-6171-learned-sort-BC-gate'):
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items(): assert f.sha(c / rel) == digest
    states = {}; final = {}; records = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = json.loads(p.read_text()); task = int(r['task'])
            assert task == int(p.stem)
            status = classify(r, folder == 'results')
            assert status['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, status)
            if folder == 'results':
                assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
                for key in ('solve', 'certificate_sha256', 'certificate_bytes'):
                    if task in records: assert records[task][key] == r[key]
                final[task] = str(p.relative_to(root))
            states[task] = status; records[task] = r
            hashes[str(p.relative_to(root))] = f.sha(p)
    count = 66 if 'BC-gate' in name else 50
    assert set(states) == set(range(count))
    if '6115' in name: assert all(s['checked'] and s['clean'] for s in states.values())
    campaigns[name] = dict(expected=count, checked_clean=sum(s['checked'] for s in states.values()),
        pending_tasks=[t for t, s in states.items() if not s['checked']],
        classes=states, manifest_sha256=f.sha(c / 'manifest.json'))
    if count == 50: campaigns[name]['score'] = f.score(c)
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), campaigns=campaigns, hashes=hashes,
    test_consideration='No new checker acceptance semantics: use fail-closed classify covered by39 benchmark tests; verify pin and provisional/final identity. Only all-checked6115 is sealed as complete; later candidate checks stay pending.',
    limitation='Certificate closure is not performance acceptance, source-fuzz closure, memory parity or causal timing attribution. Frozen score snapshots unchanged.')
target.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({n: {k: v for k, v in c.items() if k not in ('classes', 'score')} for n, c in campaigns.items()}, indent=2))
