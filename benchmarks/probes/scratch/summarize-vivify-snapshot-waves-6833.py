#!/usr/bin/env python3
"""Strict eighteen-task exact-work timing summary, no reference refresh."""
from pathlib import Path
source = Path(__file__).with_name('summarize-collapse-waves-6343.py')
code = source.read_text()
for before, after in {
    '20260909-6332-collapse-three-wave-runtime': '20260910-6831-vivify-snapshot-runtime',
    'solver-checkpoint-6343.json': 'solver-checkpoint-6833.json',
    '(32, 33, 40)': '(32, 39, 38)',
    'Changed-work candidate6305 versus6269, not isolated unit cost or fixed50 competitiveness.':
        'Same-work snapshot omission6822 versus6730 on Ranks32/39/38. Both arms match same per-rank6730 work/proof. No global scheduling/CDCL parity or fixed50 competitiveness claim. Report adverse controls; no repeated campaign to select favorable noise.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
