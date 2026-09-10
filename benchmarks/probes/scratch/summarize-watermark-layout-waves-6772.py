#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('summarize-collapse-waves-6343.py')
code = source.read_text()
changes = {
    '20260909-6332-collapse-three-wave-runtime': '20260909-6770-watermark-layout-runtime',
    'solver-checkpoint-6343.json': 'solver-checkpoint-6772.json',
    'range(18)': 'range(12)',
    'range(6*wave, 6*wave+6)': 'range(4*wave, 4*wave+4)',
    'matrix[6*wave:6*wave+6]': 'matrix[4*wave:4*wave+4]',
    '(32, 33, 40)': '(38, 46)',
    't // 6 == wave': 't // 4 == wave',
    'solver_outcomes=18': 'solver_outcomes=12',
    'Changed-work candidate6305 versus6269, not isolated unit cost or fixed50 competitiveness.':
        'Layout-only6763 versus6730 on Ranks38/46, both preserving the same per-rank work/proof oracle. No full50 competitiveness or specific microarchitectural-cause claim.'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
