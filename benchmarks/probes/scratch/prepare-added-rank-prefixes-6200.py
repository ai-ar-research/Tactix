#!/usr/bin/env python3
"""Observe the largest added-rank work losses without refreshing timing data."""
from pathlib import Path
source = Path(__file__).with_name('prepare-priority-prefixes-6139.py')
code = source.read_text()
changes = {
    '((6139, 21, 10000), (6140, 4, 100000))': '((6200, 50, 100000), (6201, 49, 100000))',
    "f'benchmarks/runs/20260908-{number}-rank{rank}-current-prefix'": "f'benchmarks/runs/20260909-{number}-rank{rank}-current-prefix'",
    'Rank4bothfreshUNSATcertificates pass;useashortcurrentprefix todistinguish changedwork fromunitcost beforeoptimizing.':
    'AddedRank50/49 are the largest added50 absolute losses,with roughly6.87/5.83 times native conflicts. First100K prefix localizes search divergence,not a unit-cost conclusion or timing-reference refresh.',
}
for old, new in changes.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
marker = "    m['hashes'] = {str(p.relative_to(c)): sha(p) for p in"
assert code.count(marker) == 1
code = code.replace(marker, """    bound = [root / f'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50/results/{rank-1:03}.json',
             root / f'benchmarks/runs/20260909-6181-learned-sort-ranks1-50/results/{rank-1:03}.json',
             root / 'benchmarks/reports/2026-09-09-rank50-loss-breakdown-6198.json']
    before, after = [json.loads(p.read_text()) for p in bound[:2]]
    nt = lambda r: [s for s in r['solver_statistics'] if not s.startswith('c timing')]
    assert before['certificate_sha256'] == after['certificate_sha256'] and nt(before) == nt(after)
    assert before['valid'] and after['valid'] and before['verdict'] == after['verdict'] == 'SAT'
    m['earlier_closure'] = {str(p.relative_to(root)): sha(p) for p in bound}
    m['scope'] = 'Trace6127 represents semantic6107;latest6169 exactly replays full work/certificate on this rank. First prefix is localization,not full-run observer neutrality. No rank-specific policy change or changed fixed reference.'
""" + marker)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
