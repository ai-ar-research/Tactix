#!/usr/bin/env python3
"""Trace the clause state that diverges before the first decision swap."""
from pathlib import Path
source=Path(__file__).with_name('prepare-rank46-allscan-reason-6702.py')
code=source.read_text()
changes={
    '20260909-6696-rank46-reason-history':'20260909-6711-rank46-implication-history',
    '20260909-6702-rank46-allscan-reason':'20260909-6721-rank46-earlier-allscan',
    'localization-6699.json':'localization-6717.json','summary-6698.json':'summary-6713.json',
    '495210':'20049','tactix-6696-':'tactix-6711-','tactix-6702-':'tactix-6721-',
    '106600':'61030','108170':'107106','107350':'73636','-16365':'-18659','19005':'18623',
    '500000':'2000000','6696-rank46-reason-history':'6711-rank46-implication-history','6702-rank46-allscan-reason':'6721-rank46-earlier-allscan',
    'ReasonB last selected-18659 state matches106604, first differs107349 after20049 complete matching portable rows. Observe all scans whose clause contains external variable18623 to capture intervening movements. Extra clauses are retained; interpret the exact reasonB key, not variable membership alone.':
        '6717 first20049 selected-scan rows match. ReasonA last matches61030; current observes it at73636 where native has no matching selected-scan row, before first decision swap78797. ReasonB pose already differs76396. Observe all scans of clauses containing18623 across61030..73636 and follow exact reasonA key; do not attribute earlier clause drift to the later branch swap.',
    'No solver edit. Existing watch payload and history tests apply; new multi-scan projection logic needs focused tests before interpretation.':
        'Configuration-only reuse of tested multi-scan projection;218 benchmark tests pass. No additional parser logic or solver change; future source repair requires focused regression.'}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
