#!/usr/bin/env python3
"""Extend the selected reason watch history without changing solver code."""
from pathlib import Path
source=Path(__file__).with_name('prepare-rank46-watch-history-6684.py')
code=source.read_text()
changes={
    '2026-09-09-congruence-runtime-rank46-audit-6679.json':'2026-09-09-rank46-watch-history-audit-6691.json',
    '20260909-6681-rank46-conflict-watch':'20260909-6693-rank46-reason-watch',
    '20260909-6684-rank46-watch-history':'20260909-6696-rank46-reason-history',
    'summary-6683.json':'summary-6695.json',
    'tactix-6681-':'tactix-6693-','tactix-6684-':'tactix-6696-',
    '108330':'108080','108341':'108170','250000':'500000',
    '6681-rank49-prefix':'6693-rank46-reason-watch','6684-rank46-watch-history':'6696-rank46-reason-history',
    'Locate first observed relative-order or pose difference for the two all-false Rank46 conflict clauses before108338. Strict watch-only snapshots of the complete-16719 scan list from startup; do not refresh timing references or change solver code.':
        'Locate first selected-scan state difference for two unit12-literal reasons of16358. At108084 current positions20/32 reverse native34/19, both reasons unit, poses and searched differ. Strict watch-only complete-16365 list from startup; not a timing reference refresh.',
    'No solver edits; use existing fixed pins and strict watch hooks.202 maintained benchmark tests cover framing/replay; add tests for any new history grouping or projection logic before interpretation.':
        'No solver edits or new history algorithm.207 maintained benchmark tests cover parameterized scan framing, exact projection, duplicates and state grouping. Existing history fixtures apply unchanged; no new unit test needed for this configuration-only use.'}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
code=code.replace('old6681 watch row','old6693 watch row')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
