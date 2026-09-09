#!/usr/bin/env python3
"""Three paired Rank49 waves with exact per-arm work and certificate checks."""
from pathlib import Path
source = Path(__file__).with_name('summarize-material-waves-6597.py')
code = source.read_text()
for before, after in {
    '20260909-6592-rank50-material-three-wave-runtime': '20260909-6663-rank49-congruence-empty-runtime',
    'solver-checkpoint-6597.json': 'solver-checkpoint-6672.json',
    "'(50,)'": "'(49,)'",
    'Changed-work material6579 versus6424 on Rank50': 'Changed-work gate-free-cleanup6654 versus6579 on Rank49',
}.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
