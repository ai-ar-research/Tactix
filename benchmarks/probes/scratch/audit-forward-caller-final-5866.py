#!/usr/bin/env python3
"""New audit after full runtime arrival; preserve the pre-score checkpoint."""
from pathlib import Path

here = Path(__file__).resolve().parent
path = here / 'audit-forward-caller-5865.py'
code = path.read_text()
for old, new in {
    '2026-09-08-forward-caller-validation.json': '2026-09-08-forward-caller-final-validation.json',
    'range(5848, 5866)': 'range(5848, 5867)',
    "'benchmarks/reports/2026-09-08-forward-caller-checkpoint.json']":
        "'benchmarks/reports/2026-09-08-forward-caller-checkpoint.json', 'benchmarks/reports/2026-09-08-forward-caller-runtime.md', 'benchmarks/reports/2026-09-08-forward-caller-validation.json']",
    'BC solver screen improves both tiers;full40 incomplete.':
        'BC improves both tiers;full40 complete7737.487s=1.136184x fixedKissat,essentially unchanged. Prior audit counts had just reached40 while its pre-score outcome text still said incomplete;this final audit supersedes that stale sentence.'}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(path), 'exec'), {'__file__': str(path), '__name__': '__main__'})
