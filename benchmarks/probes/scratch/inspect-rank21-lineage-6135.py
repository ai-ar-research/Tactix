#!/usr/bin/env python3
"""Locate changed-work regressions using existing checked normal campaigns."""
import csv
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha

numbers = (5395, 5423, 5434, 5454, 5465, 5485, 5535, 5604,
           5664, 5712, 5758, 5811, 5858, 5908, 5970, 6061, 6115, 6121)
target = root / 'benchmarks/runs/20260908-6135-rank21-lineage'
assert not target.exists()
rows = []
hashes = {}
for number in numbers:
    paths = list((root / 'benchmarks/runs').glob(f'2026090[78]-{number}-*'))
    assert len(paths) == 1, (number, paths)
    c = paths[0]
    m = json.loads((c / 'manifest.json').read_text())
    with (c / 'matrix.tsv').open(newline='') as stream:
        matrix = [r for r in csv.DictReader(stream, delimiter='\t')
                  if r['rank'] == '21' and r['arm'] in ('candidate', 'tactix')]
    assert len(matrix) == 1, number
    task = int(matrix[0]['task'])
    path = c / 'results' / f'{task:03d}.json'
    r = json.loads(path.read_text())
    assert classify(r, True)['checked'] and r['verdict'] == 'SAT', path
    assert r['instance'] == matrix[0]['instance'] and r['rank'] == 21 and r['task'] == task
    arm = m['arms'][r['arm']]
    assert r['binary_sha256'] == arm['sha256'] == sha(c / arm['path']), path
    for p in (c / 'manifest.json', c / 'matrix.tsv', path):
        hashes[str(p.relative_to(root))] = sha(p)
    statistics = '\n'.join(r['solver_statistics'])
    def count(pattern):
        matches = re.findall(pattern, statistics, re.M)
        assert len(matches) == 1, (number, pattern)
        return int(matches[0])
    rows.append(dict(campaign=number, arm=r['arm'], binary_sha256=arm['sha256'],
        source_build=arm.get('source_build'), seconds=r['solve']['wall_seconds'],
        peak_rss_kib=r['solve']['peak_rss_kb'],
        conflicts=count(r'^c conflicts\s+(\d+)'),
        decisions=count(r'^c decisions\s+(\d+)'),
        propagations=count(r'^c propagations\s+(\d+)'),
        bve_resolutions=count(r'^c bve\s+.*?, (\d+) resolutions'),
        certificate_bytes=r['certificate_bytes'], certificate_sha256=r['certificate_sha256'],
        statistics=r['solver_statistics'], record=str(path.relative_to(root))))
assert len({json.loads((root / row['record']).read_text())['instance'] for row in rows}) == 1
target.mkdir()
(target / 'summary.json').write_text(json.dumps(dict(rows=rows, hashes=hashes,
    decision='Identify the first sampled changed-work jump onrank21 before attributing current cumulative memory/runtime cost.',
    limitation='Historical different windows,not paired timing orcausal attribution toan individual patch. All selected models independently checked andclean. Sameinstance andbinaryidentity verified. Use intervening sources tolocalize,not best-of selection.'), indent=2) + '\n')
print('campaign seconds RSS_KiB conflicts decisions propagations bve_resolutions')
for row in rows:
    print(*(row[k] for k in ('campaign', 'seconds', 'peak_rss_kib', 'conflicts', 'decisions', 'propagations', 'bve_resolutions')))
