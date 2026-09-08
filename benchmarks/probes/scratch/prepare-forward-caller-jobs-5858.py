#!/usr/bin/env python3
"""New immutable BC and diagnostic campaigns; retain all predecessor runners."""
from pathlib import Path

here = Path(__file__).resolve().parent
source = here / 'prepare-forward-admission-jobs-5836.py'
code = source.read_text()
replacements = {
    '20260908-5830-forward-admission': '20260908-5849-forward-caller',
    '20260908-5806-forward-unit-BC-gate': '20260908-5836-forward-admission-BC-gate',
    '20260908-5836-forward-admission-BC-gate': '20260908-5855-forward-caller-BC-gate',
    '5836-forward-admission-BC': '5855-forward-caller-BC',
    "('B', 5830, 'release'), ('C', 5831, 'bc')": "('B', 5849, 'release'), ('C', 5850, 'bc')",
    'pin-{number}-forward-admission': 'pin-{number}-forward-caller',
    'isolated C2 admission producer5830': 'post-forward caller closure5849',
    'Native clause-addition-only C2 marks preserve expected verdicts and improve cumulative work':
        'Full connected propagation before BVE preserves expected verdicts and reduces excess search work',
    '20260908-5837-forward-admission-rank32-prefix': '20260908-5857-forward-caller-rank32-prefix',
    'pin-5832-forward-admission-trace': 'pin-5851-forward-caller-trace',
    'tactix-5837-': 'tactix-5857-',
    '5837-admission-rank32': '5857-caller-rank32',
    'Remove root-only C2 admission producer;locate earliest changed work and compare with native. Earlier divergences must be reported,not hidden by the179463 witness.':
        'Repair full connected caller closure after partial C2;compare every decision from0 to cached native and report the earliest change. No runtime claim from this diagnostic.'}
# Simultaneous replacement keeps the predecessor campaign distinct from its successor.
import re
code = re.sub('|'.join(re.escape(k) for k in sorted(replacements, key=len, reverse=True)),
              lambda m: replacements[m.group()], code)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
source = here / 'prepare-forward-admission-rank31-5845.py'
code = source.read_text()
replacements = {
    '20260908-5810-forward-unit-rank31-prefix': '20260908-5845-forward-admission-rank31-prefix',
    '20260908-5830-forward-admission': '20260908-5849-forward-caller',
    '20260908-5845-forward-admission-rank31-prefix': '20260908-5856-forward-caller-rank31-prefix',
    'pin-5832-forward-admission-trace': 'pin-5851-forward-caller-trace',
    'tactix-5810-': 'tactix-5845-', 'tactix-5845-': 'tactix-5856-',
    '5845-admission-rank31': '5856-caller-rank31',
    '5830 raises Rank31 conflicts from1.403354x to2.106051x native;locate earliest divergence versus5798 and cached native before attributing the regression to missing post-C2 caller closure.':
        'Post-forward reconnect/propagate/cleanup should close the179823 caller boundary. Compare every decision to5830 and native; first divergence is evidence,not a runtime acceptance gate.'}
code = re.sub('|'.join(re.escape(k) for k in sorted(replacements, key=len, reverse=True)),
              lambda m: replacements[m.group()], code)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
