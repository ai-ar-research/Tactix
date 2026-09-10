#!/usr/bin/env python3
"""Apply the existing exact-replay observer oracle to each new narrow window."""
from pathlib import Path
source = Path(__file__).with_name('validate-rank49-search-window-6641.py')
original = source.read_text()
for window in ('early','late'):
    code = original
    changes = {
        '20260909-6639-rank49-search-window':'20260910-6804-rank21-analysis-admission/'+window,
        'summary-6641.json':'summary-6806.json',
        "assert extra['extra_counts'] == dict(trace_begin=1, retier=15, checkpoint=90)":
            "assert set(extra['extra_counts']) <= {'trace_begin','retier','checkpoint'}",
        "assert r['partial_certificate'] == old['partial_certificate']":
            "assert r['partial_certificate'] == old['partial_certificate']\n    if arm == 'kissat': assert r['prior_certificate_prefix'] == old['prior_certificate_prefix']",
        "for name in (arm + '.decisions.tsv', arm + '.prefix.tsv'):":
            "for name in (arm + '.decisions.tsv', arm + '.prefix.tsv', arm + '.state.tsv', arm + '.bumps.tsv'):",
        "assert all(any(r['event'] == 'conflict' for r in rr) for rr in selected.values())":
            "assert all(any(r['event'] == 'conflict' for r in rr) for rr in selected.values())\nfor arm,rr in selected.items():\n    for conflict in range(m['search_window'][0],m['search_window'][1]+1):\n        events = ('analysis_begin','analysis_end','learned') + (('analyzed',) if arm=='candidate' else ())\n        for event in events:\n            assert any(r['conflicts']==conflict and r['event']==event for r in rr),(arm,conflict,event)",
        'rank=49, stop=100000':'rank=21, stop=50000',
        'Manifest expected current macro byte identity; global tracing instead adds106 source-backed macro rows: trace_begin1,retier15,checkpoint90. The new tested subsequence validator proves every22699 original row unchanged except sequence shifts; extras retained in full. Native allows only analysis/end within the declared window. No solver failure or relaxed certificate oracle.':
            'Existing CONTROL_ONLY admits bounded analysis and reason-side records. Unreachable finite event stop suppresses unrelated snapshots;50K prefix alone stops. Every original6798 decision/prefix/state/bump trace and partial proof exact; macro and native analysis/end observer additions separately accounted. No causal claim from a raw row mismatch.'}
    for before,after in changes.items():
        assert before in code,before
        code=code.replace(before,after)
    target=Path(__file__).resolve().parents[3]/('benchmarks/runs/20260910-6804-rank21-analysis-admission/'+window+'/summary-6806.json')
    if target.exists():
        print(window,'already validated; frozen writer not rerun');continue
    exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
