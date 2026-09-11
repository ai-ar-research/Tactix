#!/usr/bin/env python3
"""Conserve all phases; report whole Eliminate separately from total inprocessing."""
from pathlib import Path
base = Path(__file__).with_name('extract-vivify-compact-wave-7153.py')
code = base.read_text()
for before, after in {
    'validate-vivify-compact-profiles-7152.py':'validate-occurrence-profiles-7259.py',
    '20260910-7153-vivify-compact-samples':'20260911-7260-occurrence-samples',
    "'tactix::vivify_apply('":"'tactix::DefaultInprocessScheduler::run_phase('",
    "    for phase_name, components in summary['components'].items():\n        assert envelope['by_phase'].get(phase_name, 0) == components.get('vivify', 0), phase_name":
        "    assert set(envelope['by_phase']) == {'initial_probe', 'probe', 'eliminate'}\n    for phase_name in envelope['by_phase']:\n        assert envelope['by_phase'][phase_name] == summary['phase_periods'][phase_name]\n    assert envelope['outside_period'] == summary['phase_periods'].get('no_phase_frame', 0)",
    'complete Vivify cycles':'complete inprocessing cycles',
    'complete_vivify_ratio=':"complete_eliminate_ratio=ratios['eliminate'],\n        complete_inprocess_ratio=",
}.items():
    assert code.count(before) == 1,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
