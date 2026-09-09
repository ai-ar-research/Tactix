#!/usr/bin/env python3
"""Predeclare productive32/33 and largest adverse35 from the complete50 view."""
from pathlib import Path
import json
root = Path(__file__).resolve().parents[3]
summary = root/'benchmarks/reports/2026-09-09-compact-stamp-full50-6371/summary.json'
s = json.loads(summary.read_text())
assert s['score']['expected_solves'] == 50
assert max(s['rows'], key=lambda r:r['delta_parent_seconds'])['rank'] == 35
source = Path(__file__).with_name('prepare-collapse-timing-6332.py')
code = source.read_text()
for old, new in (
    ('20260909-6332-collapse-three-wave-runtime', '20260909-6374-compact-stamp-three-wave-runtime'),
    ('20260909-6311-substitute-collapse-BC-gate', '20260909-6362-compact-stamp-BC-gate'),
    ("('candidate',6305,'substitute-collapse','20260909-6320-substitute-collapse-ranks1-50')",
     "('candidate',6357,'compact-stamp','20260909-6366-compact-stamp-ranks1-50')"),
    ("('previous',6269,'vivify-binary-retry','20260909-6278-vivify-binary-ranks1-50')",
     "('previous',6305,'substitute-collapse','20260909-6320-substitute-collapse-ranks1-50')"),
    ('32,33,40', '32,33,35'),
    ("'40':'observed adverse search-work control'", "'35':'largest observed adverse runtime change in the complete50 view'"),
    ('6332-collapse-timing', '6374-compact-stamp-timing'),
    ('tactix-collapse-changed-work-three-waves-v1', 'tactix-compact-stamp-changed-work-three-waves-v1'),
    ('6305 retained-endpoint retry causes repeatable changed-work runtime effects versus6269, including the adverseRank40 case.',
     '6357 detached queue-record preservation causes repeatable changed-work effects versus6305: primary Rank32, productive Rank33 and largest adverse Rank35. Full50 is1.202558x, not a demonstrated aggregate gain.'),
    ('Unchanged pinned runner/oracles; existing39 benchmark13watcher tests. Matrix validates18tasks,pairedarmcoverage; source gates105/104,72fixtures,8proofs alreadypassed. New timing compares different search work,not isolated instruction cost.',
     'Unchanged pinned runner/oracles;63benchmark13watcher tests. Matrix validates18tasks and paired coverage. Current source106Release twice105trace plus24fixed/12native controls, B/C66expected. Different-work pricing, not instruction-cost isolation.'),
):
    assert old in code, old
    code = code.replace(old, new)
marker = "m['pin_sha256']="
assert code.count(marker) == 1
code = code.replace(marker, "m['selection_authority']={'path':str(summary.relative_to(root)),'sha256':sha(summary)}\n"+marker)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__', 'summary':summary})
