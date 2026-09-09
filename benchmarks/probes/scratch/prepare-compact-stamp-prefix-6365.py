#!/usr/bin/env python3
from pathlib import Path
import json
root = Path(__file__).resolve().parents[3]
gate = root/'benchmarks/runs/20260909-6362-compact-stamp-BC-gate/solver-checkpoint-6364.json'
assert json.loads(gate.read_text())['solver_records'] == 66
source = Path(__file__).with_name('prepare-collapse-prefix-6321.py')
code = source.read_text()
for before, after in (
    ('20260909-6321-collapse-rank32-prefix', '20260909-6365-compact-stamp-rank32-prefix'),
    ('20260909-6305-substitute-collapse-build', '20260909-6357-compact-stamp-build'),
    ('build/pin-6305-substitute-collapse-', 'build/pin-6357-compact-stamp-'),
    ('tactix-6321-', 'tactix-6365-'),
    ('6321-collapse-prefix', '6365-compact-stamp-prefix'),
    ('Current6305', 'Current6357'),
    ('current Substitute collapse repair', 'current detached queue-stamp repair'),
    ('Retained endpoint retry can change later BVE scheduling; no assumed causal or performance benefit.',
     'Preserving the fixed representative stamp should restore transitive scheduling at1368343; test inherited1M prefix first, then the full3M stream. An earlier changed prefix invalidates this localization, not solver correctness.'),
):
    assert before in code, before
    code = code.replace(before, after)
marker = "m['resource_contract']['array']='0'"
assert code.count(marker) == 1
code = code.replace(marker, "m['provisional_gate']={'path':str(gate.relative_to(root)),'sha256':sha(gate)}\n"+marker)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__', 'gate': gate})
