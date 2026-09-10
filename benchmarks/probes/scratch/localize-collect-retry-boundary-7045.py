#!/usr/bin/env python3
"""Reuse the bound boundary reader; verify the selected BVE effect now matches."""
from pathlib import Path
source = Path(__file__).with_name('localize-rank35-native-boundary-7017.py')
code = source.read_text()
changes = {
    "c = root / 'benchmarks/runs/20260910-7011-rank35-current-native-prefix'":
        "c = root / 'benchmarks/runs/20260910-7038-collect-retry-prefix'\nparent = root / 'benchmarks/runs/20260910-7011-rank35-current-native-prefix'",
    'boundary-localization-7017.json': 'boundary-closure-7045.json',
    "p = c / 'summary-7015.json'": "p = c / 'summary-7040.json'",
    "assert s['matching_rows_from_start'] == 79224352\nassert all(s['inherited100k'].values())":
        "assert s['inherited100k'] and s['comparisons']['kissat']['complete_equal']\nassert s['comparisons']['kissat']['matching_rows_from_start'] == 100845524",
    "p = c / 'traces' / (arm + '.boundaries.tsv')":
        "p = (c if arm == 'candidate' else parent) / 'traces' / (arm + '.boundaries.tsv')",
    "assert selected['candidate'][3] != selected['kissat'][3]":
        "assert selected['candidate'] == selected['kissat']",
    "preparation_failure='7016 incorrectly asserted the first projected count mismatch would coincide with the selected pre-decision BVE phase. No summary written. Fresh7017 preserves earlier mismatches instead of dropping them; first raw mismatch is not automatically a worse-work boundary.'":
        "reused_reader='Frozen7017 boundary schema/projection reused. Earlier Sweep copy mismatch retained; selected387253 BVE effects now agree, as does complete1M decision prefix.'",
    'Compare selected first BVE physical schedule and all candidate order/effects/budget at387253. The extra eliminations are not inherently harmful; establish the cause and its downstream consequence before repair.':
        'Selected first and second BVE boundary counts now match native. Await predeclared paired runtime waves; no further heap trace needed merely to reproduce parity.',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
