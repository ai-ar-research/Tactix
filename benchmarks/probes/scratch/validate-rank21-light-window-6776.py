#!/usr/bin/env python3
"""Full own50K replay before interpreting a compact conflict trajectory."""
from pathlib import Path
source = Path(__file__).with_name('validate-rank49-search-window-6641.py')
code = source.read_text()
changes = {
    '20260909-6639-rank49-search-window': '20260909-6774-rank21-light-window',
    'summary-6641.json': 'summary-6776.json',
    "assert extra['extra_counts'] == dict(trace_begin=1, retier=15, checkpoint=90)":
        "assert set(extra['extra_counts']) <= {'trace_begin', 'retier', 'checkpoint'}",
    "assert r['partial_certificate'] == old['partial_certificate']":
        "assert r['partial_certificate'] == old['partial_certificate']\n    if arm == 'kissat':\n        assert r['prior_certificate_prefix'] == old['prior_certificate_prefix']",
    'rank=49, stop=100000': 'rank=21, stop=50000',
    'Manifest expected current macro byte identity; global tracing instead adds106 source-backed macro rows: trace_begin1,retier15,checkpoint90. The new tested subsequence validator proves every22699 original row unchanged except sequence shifts; extras retained in full. Native allows only analysis/end within the declared window. No solver failure or relaxed certificate oracle.':
        'Existing trajectory-only mode excludes Sweep noise and analysis snapshots; finite event-stop one past50K suppresses intermediate macro/heap snapshots. Independent50K prefix remains authoritative. Original decisions, partial proof and prefixes exact; candidate macro additions and native analysis/end additions explicitly bounded. No implication or ranking-snapshot coverage claimed.'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
