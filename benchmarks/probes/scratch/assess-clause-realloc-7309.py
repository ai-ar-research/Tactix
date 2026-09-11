#!/usr/bin/env python3
"""Assess all three declared clause-arena runtime/RSS waves, without selection."""
from pathlib import Path
base = Path(__file__).with_name('assess-dense-realloc-7281.py')
code = base.read_text()
changes = {
    '20260911-7273-dense-realloc-runtime': '20260911-7306-clause-realloc-runtime',
    '2026-09-11-dense-realloc-assessment-7281.json': '2026-09-11-clause-realloc-assessment-7309.json',
    'check-submit-dense-realloc-runtime-7273.py': 'check-submit-clause-realloc-runtime-7306.py',
    'Every predeclared pair retained. Prioritize9/25 peak-RSS benefit, but explicitly retain31 memory and38 outside-slot regressions. Raw counters/certificates exact, including physical dense moves. Paired median/full range and absolute-headline2% spread gate unchanged. No overall win inferred from memory-only improvements.':
        'Every predeclared pair retained. Test reduction of Rank31 clause-arena-growth peak, preserving Rank9/25 dense savings, and retain every runtime/RSS regression on all five witnesses. Exact raw work/certificates include physical counters. Paired medians/full ranges and the2% absolute-headline stability gate remain unchanged. Memory-only gains are not an overall runtime win.',
    'Combine the complete repeated tradeoff with7277 current peak ownership and7278/7279/7280 Rank38 code-cost evidence. Decide whether a complete full50 memory/runtime transfer or a principled repair is justified. No alignment tuning or favorable-repeat selection.':
        'Compare all repeated tradeoffs with exact-Release peak localization7295/7299/7300. Decide whether the clause-arena change warrants a fresh full50 transfer; do not attribute retained heap changes or instruction cost from RSS/timing alone. Diagnose any decision-changing regression using the actual pins. No alignment tuning, favorable-repeat selection or gate reduction.',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
