#!/usr/bin/env python3
"""Check completion and classify the exact-pair binary producer rows."""
from collections import Counter
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
m = json.loads((C/'manifest.json').read_text())
summary = {}
for task, arm in enumerate(('core', 'sequential')):
    r = json.loads((C/'results'/f'{task:03d}.json').read_text())
    assert r['task'] == task and r['arm'] == arm and r['rank'] == 21
    assert r['valid'] and r['solve']['exit_code'] == 10 and not r['solve']['timed_out']
    assert r['model_check']['result'] == 'verified' and r['model_check']['exit_code'] == 0
    assert r['artifact_deleted'] and not r['cleanup_pending']
    assert r['binary_sha256'] == m['arms'][arm]['sha256']
    boundary = (C/'traces'/f'{arm}.tsv').read_bytes()
    previous = (C.parent/'20260907-5333-rank21-standalone'/'traces'/f'{arm}.tsv').read_bytes()
    assert boundary == previous, 'Lifecycle observer changed recorded trajectory'
    raw = (C/'traces'/f'{arm}.lifecycle.tsv').read_bytes()
    rows = [line.split('\t') for line in raw.decode().splitlines() if not line.startswith('#')]
    target = [row for row in rows if row[4] == '23889']
    events = Counter(row[2] for row in target)
    assert events['add-new'] == 4 and not events['sweep-large-collapse-source']
    assert events['sweep-physical-target'] == events['add-copy'] == (4 if arm == 'sequential' else 0)
    assert all(row[9:11] == ['0', '0'] for row in target)
    assert all(row[12] == '2' for row in target if row[2] == 'add-copy')
    summary[arm] = dict(total_rows=len(rows), target_rows=len(target), events=events,
                        sha256=hashlib.sha256(raw).hexdigest(), target_records=target)
print(json.dumps(dict(checked_clean=2, boundary_traces_identical_to_5333=True,
    arms=summary,
    conclusion='Each pair is created once in both arms. Only sequential subsequently rebuilds one additional physical copy from a binary image. No selected large-clause collapse; endpoints unassigned at all selected events.',
    limitation='Exact source endpoints are not in target-only lifecycle rows. Correlate source-target rewrite or emitted equivalences before claiming why the extra copy exists or that removing it fixes runtime.'), indent=2))
