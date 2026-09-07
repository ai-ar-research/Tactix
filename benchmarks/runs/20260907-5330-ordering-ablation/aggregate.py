#!/usr/bin/env python3
"""Require all six checked variants before locating the changed-work arm."""
import hashlib
import json
from pathlib import Path
import re

C = Path(__file__).resolve().parent
manifest = json.loads((C / 'manifest.json').read_text())
rows = [json.loads((C / 'results' / f'{i:03d}.json').read_text()) for i in range(6)]
result = []
for i, row in enumerate(rows):
    assert row['task'] == i and row['rank'] == 21
    assert row['valid'] and row['artifact_deleted'] and not row['cleanup_pending']
    assert row['solve']['exit_code'] == 10 and not row['solve']['timed_out']
    assert row['model_check']['result'] == 'verified' and row['model_check']['exit_code'] == 0
    assert row['binary_sha256'] == manifest['arms'][row['arm']]['sha256']
    text = '\n'.join(row['solver_statistics'])
    counts = {}
    for key in ('conflicts', 'decisions', 'propagations'):
        counts[key] = int(re.search(r'^c\s+' + key + r'\s+(\d+)', text, re.M)[1])
    result.append(dict(arm=row['arm'], counters=counts, seconds=row['solve']['wall_seconds'],
                       rss_kib=row['solve']['peak_rss_kb']))
baseline = result[0]['counters']
print(json.dumps(dict(checked_clean=6, rows=result,
    changed_work_arms=[r['arm'] for r in result if r['counters'] != baseline],
    caveat='Counter differences select the next causal trace; one wave does not establish timing acceptance or a matched trajectory.',
    result_sha256={str(p.relative_to(C)): hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in sorted((C / 'results').glob('*.json'))}), indent=2))
