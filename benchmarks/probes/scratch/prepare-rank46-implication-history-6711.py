#!/usr/bin/env python3
"""Trace the history of the reordered implication watch list."""
from pathlib import Path
source=Path(__file__).with_name('prepare-rank46-watch-history-6684.py')
code=source.read_text()
changes={
    '2026-09-09-congruence-runtime-rank46-audit-6679.json':'2026-09-09-rank46-reason-history-audit-6700.json',
    '20260909-6681-rank46-conflict-watch':'20260909-6708-rank46-implication-watch',
    '20260909-6684-rank46-watch-history':'20260909-6711-rank46-implication-history',
    'summary-6683.json':'summary-6710.json',
    'tactix-6681-':'tactix-6708-','tactix-6684-':'tactix-6711-',
    '108330':'107100','108341':'107106',
    '6681-rank49-prefix':'6708-rank46-implication-watch','6684-rank46-watch-history':'6711-rank46-implication-history',
    'Locate first observed relative-order or pose difference for the two all-false Rank46 conflict clauses before108338. Strict watch-only snapshots of the complete-16719 scan list from startup; do not refresh timing references or change solver code.':
        'Locate first observed ordering or state difference for the two14-literal reasons of18623 and18609, whose implication order reverses107104. Strict watch-only snapshots of complete-18659 scan list from startup. Shared18626 in these clauses does not itself establish any causal link to the earlier decision swap at78797.',
    'No solver edits; use existing fixed pins and strict watch hooks.202 maintained benchmark tests cover framing/replay; add tests for any new history grouping or projection logic before interpretation.':
        'No solver edit or new history algorithm;213 maintained benchmark tests pass. Existing history grouping/projection fixtures apply unchanged. Add a focused regression for any subsequent source repair.'}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
code=code.replace('old6681 watch row','old6708 watch row')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
