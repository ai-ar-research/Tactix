#!/usr/bin/env python3
"""Summarize all six checked records; never treat pending checks as success."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
rows = [json.loads((C / 'results' / f'{i:03d}.json').read_text()) for i in range(6)]
assert all(r['valid'] and r['artifact_deleted'] and not r['cleanup_pending'] for r in rows)
assert all(r['model_check']['result'] == 'verified' for r in rows)
pairs = []
for i in range(0, 6, 2):
    base, candidate = rows[i:i+2]
    assert base['rank'] == candidate['rank'] and base['arm'] == 'shipped' and candidate['arm'] == 'candidate'
    pairs.append(dict(rank=base['rank'], shipped_seconds=base['solve']['wall_seconds'],
                      candidate_seconds=candidate['solve']['wall_seconds'],
                      ratio=candidate['solve']['wall_seconds']/base['solve']['wall_seconds'],
                      rss_kib=[base['solve']['peak_rss_kb'], candidate['solve']['peak_rss_kb']],
                      certificate_bytes=[base['certificate_bytes'], candidate['certificate_bytes']],
                      same_certificate=base['certificate_sha256']==candidate['certificate_sha256'],
                      statistics={r['arm']: r['solver_statistics'] for r in (base,candidate)}))
print(json.dumps(dict(checked=6, invalid=0, scratch_clean=6, pairs=pairs,
                     scope='Single-wave diagnostic, not decision-grade performance evidence',
                     result_sha256={str(p.relative_to(C)): hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in sorted((C/'results').glob('*.json'))}), indent=2))
