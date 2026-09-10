#!/usr/bin/env python3
"""Localize the validated implication-watch history with portable rows."""
from pathlib import Path
source=Path(__file__).with_name('analyze-rank46-watch-history-6687.py')
code=source.read_text()
changes={
    '20260909-6684-rank46-watch-history':'20260909-6711-rank46-implication-history',
    'localization-6687.json':'localization-6717.json','summary-6686.json':'summary-6713.json',
    '-16719':'-18659','108341':'107106',
    "assert first['index']==177292\nassert first['candidate']['conflicts']==first['kissat']['conflicts']==108138\nassert first['candidate']['decisions']==first['kissat']['decisions']==402320":
        "assert first['candidate'] is not None and first['kissat'] is not None\nassert first['index']>0",
    'First177292 selected-scan portable rows agree from startup. At108138/402320/level19, the first difference is a32-literal clause pose swapping-18994 and18996, both false; clause identity,blocker,searched and values agree. The eventual21-literal conflict clause first differs at108162, after a matching observation108086. Both eventual conflict clauses initially agree afterBVE99580 and through102107; no basis to blame their creation or blindly reorder watches.':
        'first_global_difference and selected_clauses retain the earliest observed portable differences, not an inferred cause. Compare the recorded states with the independently replay-validated earliest decision-swap window before drawing a causal link.',
    'Capture complete conflict/implication detail108080..108170 on the same pins; require own1M decisions/partial proof replay. This brackets the first observed selected-scan pose change, not necessarily the earliest global divergence.':
        'Trace the earliest differing selected-clause state through other scans or earlier search as indicated. Do not sort watches or alter branch policy without a source-backed cause and regression.'}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
