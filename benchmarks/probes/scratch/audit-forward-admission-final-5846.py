#!/usr/bin/env python3
"""New final audit after full runtime arrival; preserve the earlier checkpoint."""
from pathlib import Path

here = Path(__file__).resolve().parent
path = here / 'audit-forward-admission-5841.py'
program = path.read_text()
replacements = {
    "target = root / 'benchmarks/reports/2026-09-08-forward-admission-validation.json'":
        "target = root / 'benchmarks/reports/2026-09-08-forward-admission-final-validation.json'",
    "jobs = '1673402,1673704,1673816,1673996,1674055,1674056,1674132'":
        "jobs = '1673402,1673704,1673816,1673996,1674055,1674056,1674132,1674241'",
    "for number in range(5810, 5842):": """pending = root / 'benchmarks/runs/20260908-5845-forward-admission-rank31-prefix'
pm = json.loads((pending / 'manifest.json').read_text())
for rel, digest in pm['hashes'].items():
    assert sha(pending / rel) == digest
    if (pending / rel).suffix in ('.py', '.sbatch'):
        retain(pending / rel)
for p in pending.glob('*.json'):
    retain(p)
retain(root / 'benchmarks/reports/2026-09-08-forward-admission-validation.json')
retain(root / 'benchmarks/reports/2026-09-08-forward-admission-runtime.md')
for number in range(5810, 5847):""",
    'report-to-andrew/20260908-1330-forward-admission': 'report-to-andrew/20260908-1342-forward-admission-runtime',
    '5830 implemented/tested/unfuzzed;Rank32 first split moved to225987 after61,154,590 common decisions. Full5838 andBC5836 ongoing;goal not met or promoted.':
        '5830 tested/unfuzzed;Rank32 prefix closes61,154,590 decisions. Full5838 all40 solves1.137205x,not an aggregate win;BC C1.032997x. Checks run;Rank31 regression diagnostic5845/job1674241 submitted. Goal unmet,no promotion.'}
for old, new in replacements.items():
    assert old in program, old
    program = program.replace(old, new)
exec(compile(program, str(path), 'exec'), {'__file__': str(path), '__name__': '__main__'})
