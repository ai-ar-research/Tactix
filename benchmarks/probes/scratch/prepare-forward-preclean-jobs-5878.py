#!/usr/bin/env python3
"""Create new BC and Rank31 campaigns, without touching submitted predecessors."""
from pathlib import Path
import re

here = Path(__file__).resolve().parent
def adapt(source, code, replacements):
    code = re.sub('|'.join(re.escape(k) for k in sorted(replacements, key=len, reverse=True)),
                  lambda m: replacements[m.group()], code)
    exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

source = here / 'prepare-forward-admission-jobs-5836.py'
code = source.read_text().split("old = runs / '20260908-5771-reuse-target-rank32-extended'")[0]
adapt(source, code, {
    '20260908-5830-forward-admission': '20260908-5870-forward-preclean',
    '20260908-5806-forward-unit-BC-gate': '20260908-5855-forward-caller-BC-gate',
    '20260908-5836-forward-admission-BC-gate': '20260908-5878-forward-preclean-BC-gate',
    '5836-forward-admission-BC': '5878-forward-preclean-BC',
    "('B', 5830, 'release'), ('C', 5831, 'bc')": "('B', 5870, 'release'), ('C', 5871, 'bc')",
    'pin-{number}-forward-admission': 'pin-{number}-forward-preclean',
    'isolated C2 admission producer5830': 'preconnection satisfied-clause cleanup5870',
    'Native clause-addition-only C2 marks preserve expected verdicts and improve cumulative work':
        'Removing incoming-satisfied occurrences before BVE preserves verdicts and reduces excess search work',
})
source = here / 'prepare-forward-admission-rank31-5845.py'
adapt(source, source.read_text(), {
    '20260908-5810-forward-unit-rank31-prefix': '20260908-5856-forward-caller-rank31-prefix',
    '20260908-5830-forward-admission': '20260908-5870-forward-preclean',
    '20260908-5845-forward-admission-rank31-prefix': '20260908-5879-forward-preclean-rank31-prefix',
    'pin-5832-forward-admission-trace': 'pin-5872-forward-preclean-trace',
    'tactix-5810-': 'tactix-5856-', 'tactix-5845-': 'tactix-5879-',
    '5845-admission-rank31': '5879-preclean-rank31',
    '20260908-5838-forward-admission-ranks1-40': '20260908-5858-forward-caller-ranks1-40',
    '5830 raises Rank31 conflicts from1.403354x to2.106051x native;locate earliest divergence versus5798 and cached native before attributing the regression to missing post-C2 caller closure.':
        'Correct the six BVE score residuals caused by nine stale occurrences in three incoming-root-satisfied clauses. Compare complete decisions and forward/BVE boundaries against5849 and cached native; no standalone schedule observer or native rerun.',
})
