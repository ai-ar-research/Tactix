#!/usr/bin/env python3
"""Preserve exact own50K trajectory before interpreting focused search rows."""
from pathlib import Path
source = Path(__file__).with_name('validate-rank49-search-window-6641.py')
code = source.read_text()
changes = {
    '20260909-6639-rank49-search-window': '20260909-6757-rank21-search-window',
    'summary-6641.json': 'summary-6759.json',
    "assert extra['extra_counts'] == dict(trace_begin=1, retier=15, checkpoint=90)":
        "assert extra['extra_counts'].get('trace_begin') == 1\n            assert set(extra['extra_counts']) <= {'trace_begin', 'retier', 'checkpoint'}",
    "assert r['partial_certificate'] == old['partial_certificate']":
        "assert r['partial_certificate'] == old['partial_certificate']\n    if arm == 'kissat':\n        assert r['prior_certificate_prefix'] == old['prior_certificate_prefix']",
    'rank=49, stop=100000': 'rank=21, stop=50000',
    'Manifest expected current macro byte identity; global tracing instead adds106 source-backed macro rows: trace_begin1,retier15,checkpoint90. The new tested subsequence validator proves every22699 original row unchanged except sequence shifts; extras retained in full. Native allows only analysis/end within the declared window. No solver failure or relaxed certificate oracle.':
        'Manifest permits only source-backed trace_begin/retier/checkpoint macro additions and native analysis/end rows within25098..25103. Every original row otherwise preserved; full50K decisions and partial proof exact, including native old10K proof prefix. No final model or runtime acceptance claim.'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
