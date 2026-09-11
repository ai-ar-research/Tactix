#!/usr/bin/env python3
"""Locate exact Eliminate reconstruction instructions before a fill redesign."""
from pathlib import Path
base = Path(__file__).with_name('localize-factor-pcs-7170.py')
code = base.read_text()
changes = {
    '20260910-7170-factor-pc-localization': '20260911-7208-dense-rebuild-pc-localization',
    "'tactix::factor_apply_seeded(tactix::Kernel&, unsigned int)'":
        "'tactix::rebuild_dense_large_occurrences(tactix::Kernel&, tactix::OccIndex&)'",
    "s == 'kissat_factor'": "s == 'kissat_connect_irredundant_large_clauses'",
    "if name != 'probe'": "if name != 'eliminate'",
    "expected['phase_periods']['probe']": "expected['phase_periods']['eliminate']",
    "expected['self_by_phase']['probe']": "expected['self_by_phase']['eliminate']",
    'probe_period=probe': 'eliminate_period=probe',
    'Localize the large inlined Factor loop before selecting a structural design; no solver edit or integration commit.':
        'Localize Eliminate dense reconstruction before a fill-locality redesign. The native selected connector has different responsibilities and is not a matched operation-price denominator. No solver edit or integration commit.',
}
for before,after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
