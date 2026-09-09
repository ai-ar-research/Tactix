#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('land-retry-full50-6260.py')
code = source.read_text()
for old, new in {
    '2026-09-09-retry-full50-audit-6259.json': '2026-09-09-material-schedule-audit-6590.json',
    'Record full Rank 1-50 repair outcome and remaining retry omissions':
        'Connect approved subsumption material lifecycle to C2 and Eliminate\n\nUser approved the separate byte-per-variable subsumption flag, cumulative kernel counter and staged canonical scheduling repair described in the frozen material-tracker design. This checkpoint migrates canonical C2 predicates and normal clear/rearm, retains the explicit band and public proof-rule surface, and adds Eliminate material admission with maximum-bound exit-only consumption. Shared producers and SP-SLOT-5 removal events remain unchanged. Requires independent checks, memory and full Rank1-50 gates before promotion. No FlatLists, fuzz-access or push approval inferred.',
}.items():
    assert code.count(old) == 1
    code = code.replace(old, new.replace('\n', '\\n'))
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
