#!/usr/bin/env python3
"""Map retained AND extraction samples to exact instructions before editing."""
from pathlib import Path

base = Path(__file__).with_name('localize-factor-pcs-7170.py')
code = base.read_text()
for before, after in {
    '20260910-7170-factor-pc-localization': '20260911-7195-congruence-and-pc-localization',
    "'tactix::factor_apply_seeded(tactix::Kernel&, unsigned int)'":
        "'tactix::congruence_extract(tactix::Kernel const&, std::vector<tactix::CongGate, std::allocator<tactix::CongGate> >&)'",
    "s == 'kissat_factor'": "s == 'kissat_congruence'",
    'Localize the large inlined Factor loop before selecting a structural design; no solver edit or integration commit.':
        'Localize current AND extraction before selecting a repair. The native symbol is the whole Congruence entry, not an AND-only equivalent; do not compare those self totals as matched operation costs. No solver edit or integration commit.',
}.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
