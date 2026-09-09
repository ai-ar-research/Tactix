#!/usr/bin/env python3
"""Reuse the maintained complete50 view; require exact parent work/proofs."""
from pathlib import Path
source=Path(__file__).with_name('summarize-compact-stamp-full50-6371.py')
code=source.read_text()
for old,new in {
    '20260909-6366-compact-stamp-ranks1-50':'20260909-6407-count-cache-ranks1-50',
    '20260909-6320-substitute-collapse-ranks1-50':'20260909-6366-compact-stamp-ranks1-50',
    '2026-09-09-compact-stamp-full50-6371':'2026-09-09-count-cache-full50-6418',
}.items():
    assert old in code; code=code.replace(old,new)
anchor='groups = {}'; assert code.count(anchor)==1
code=code.replace(anchor,"assert all(r['certificate_exact_parent'] and r['non_timing_stats_exact_parent'] for r in rows), 'Cache must preserve every parent work/proof row'\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
