#!/usr/bin/env python3
"""Successor checkpoint; keep the prior audit and its snapshot frozen."""
from pathlib import Path
source = Path(__file__).with_name('audit-rank31-late-bve-7071.py')
code = source.read_text()
changes = {
    '2026-09-10-rank31-late-bve-audit-7071.json': '2026-09-10-rank31-bve-priority-audit-7076.json',
    '2026-09-10-rank31-late-bve-localization.md': '2026-09-10-rank31-bve-priority.md',
    "'prepare-rank31-selected-bve-7068', 'submit-rank31-selected-bve-7069',\n    'validate-rank31-selected-bve-7070', 'audit-rank31-late-bve-7071'":
        "'summarize-rank31-bve-effects-7072', 'prepare-rank31-bve-priority-7073',\n    'submit-rank31-bve-priority-7074', 'validate-rank31-bve-priority-7075', 'audit-rank31-bve-priority-7076'",
    "1679755,1679809,1679811,1679815'": "1679755,1679809,1679811,1679815,1679819'",
    "('20260910-7068-rank31-selected-bve', 2, lambda t: f'1679815_{t}')]":
        "('20260910-7068-rank31-selected-bve', 2, lambda t: f'1679815_{t}'),\n    ('20260910-7073-rank31-bve-priority', 2, lambda t: f'1679819_{t}')]",
    "('20260910-7064-', '20260910-7068-')": "('20260910-7064-', '20260910-7068-', '20260910-7073-')",
    "probe = root / 'benchmarks/runs/20260910-7068-rank31-selected-bve'":
        """for filename in ('solver-summary-7070.json', 'effect-summary-7072.json'):
    p = root / 'benchmarks/runs/20260910-7068-rank31-selected-bve' / filename
    summary = json.loads(p.read_text()); bind(p)
    for rel, digest in summary['hashes'].items(): bind(root / rel, digest)
probe = root / 'benchmarks/runs/20260910-7073-rank31-bve-priority'""",
    "assert pm['selected_conflict'] == 5187498":
        "assert pm['selected_conflict'] == 5187498 and pm['subjects'] == [355342,355344,497326,497345]",
    '20260910-1034-rank31-bve': '20260910-1052-rank31-priority',
    "commands += [[sys.executable, 'benchmarks/probes/scratch/validate-rank31-selected-bve-7070.py', '--self-test']]":
        "commands += [[sys.executable, 'benchmarks/probes/scratch/' + name + '.py', '--self-test'] for name in ('prepare-rank31-bve-priority-7073', 'validate-rank31-bve-priority-7075')]",
    'for rel in sorted(set(owned)): bind(root / rel)':
        "bind(Path(__file__).with_name('audit-rank31-late-bve-7071.py'))\nfor rel in sorted(set(owned)): bind(root / rel)",
    'Status answer supplied newly completed solver records and preliminary boundary evidence, but no validated checkpoint. This continuation makes progress through7066 full replay, four harness tests and isolated7068 submission.':
        'Previous turn was progress:1ddcd97b validated7066, tested solver environment and submitted7068. This turn validated7070 full replay, isolated the unique lost355342 elimination with7072, and submitted7073 selected priority histories.',
    'Rank31 remains first: validate7070 full own Release and entire7064 boundary replay, then inspect selected BVE attempts at5187498. No solver edit, native rerun or promotion; largest-gap focus unchanged.':
        'Rank31 remains first.7072 rules out ceiling exhaustion in the selected phase and identifies355342 as the sole differing elimination. Validate7075 exact full own Release and entire7068 traces before inspecting incoming heap score/position and producer-triggered movement. No solver edit, native rerun, tie tuning or promotion.',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
