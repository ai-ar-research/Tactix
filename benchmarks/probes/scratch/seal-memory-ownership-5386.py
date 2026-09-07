#!/usr/bin/env python3
"""Validate checked memory observer output and rank retained ownership peaks."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5386-memory-ownership'
p = c / 'results/000.json'
if not p.exists(): print('5386 pending'); raise SystemExit(0)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, expected in m['pin_sha256'].items(): assert sha(c / rel) == expected
r = json.loads(p.read_text())
assert r['valid'] and r['expected_match'] and r['verdict'] == 'SAT'
assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
assert r['solve']['exit_code'] == 10 and not r['solve']['timed_out']
assert r['model_check']['exit_code'] == 0 and not r['model_check']['timed_out']
assert r['binary_sha256'] == m['arms']['candidate']['sha256']
old = [json.loads(f.read_text()) for f in (root / m['parent_campaign'] / 'results').glob('*.json')]
old = next(x for x in old if x['rank'] == 21 and x['arm'] == 'candidate')
def counters(rows):
    text = '\n'.join(rows)
    return {n: int(re.search(r'^c ' + n + r'\s*:?\s*(\d+)', text, re.M).group(1))
            for n in ('conflicts', 'decisions', 'propagations')}
assert counters(r['solver_statistics']) == counters(old['solver_statistics'])
trace = c / 'logs/slot-memory.tsv'
assert sha(trace) == r['solve']['memory_trace']['sha256']
rows = []
for i, line in enumerate(trace.read_text().splitlines()):
    fields = line.split('\t')
    values = {k: int(v) for k, v in (field.split('=') for field in fields[2:])}
    rows.append(dict(row=i + 1, name=fields[0], stage=fields[1], **values))
assert rows
peaks = {key: max(rows, key=lambda row: row[key]) for key in
         ('current_rss_kib', 'max_rss_kib', 'dense_capacity_bytes', 'heap_used_kib', 'heap_free_kib', 'mmap_kib')}
result = dict(final_records=1, rows=len(rows), counters=counters(r['solver_statistics']),
              result_sha256=sha(p), trace_sha256=sha(trace), peaks=peaks,
              release_peak_rss_kib=old['solve']['peak_rss_kb'], observed_peak_rss_kib=r['solve']['peak_rss_kb'],
              limitations=m['limitations'])
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print('5386 checked and clean', len(rows), 'rows; peak owners',
      {k: (v['name'], v['stage'], v[k]) for k, v in peaks.items()})
