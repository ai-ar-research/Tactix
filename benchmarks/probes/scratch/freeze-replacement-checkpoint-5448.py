#!/usr/bin/env python3
"""Extend the scoped non-fuzz audit to the new candidate and timing gates."""
from pathlib import Path

directory = Path(__file__).resolve().parent
source = (directory / 'freeze-vivify-checkpoint-5441.py').read_text()
changes = {
    '5438:None,5440:1670118,5441:1670119}':
        '5438:None,5440:1670118,5441:1670119,5445:None,5447:1670198,5448:1670199}',
    '2026-09-07-vivify-max-main-ctest.log': '2026-09-07-replacement-cursor-main-ctest.log',
    '2026-09-07-vivify-max-validation.json': '2026-09-07-replacement-cursor-validation.json',
    '20260907-2144-vivify-max': '20260907-2205-replacement-cursor',
    'seconds=5.22': 'seconds=5.33',
    'Production unchanged. Vivify 5438 preserves actual old/new order and pose in 144 cases; matching builds pass 79/79. Runtime/BC queued; Claude-owned background fuzz pending; no promotion.':
        'Production unchanged. Phase-entry full solver view 1.205579786x. Replacement cursor 5445 preserves actual old/new state in 840 cases; matching builds pass 80/80. Runtime/BC queued; current Claude fuzz covers predecessor only; 5445 UNFUZZED, no promotion.',
    'User authorized periodic background fuzzing during independent development; plan updated. Separate future solver/checker jobs remain unapproved; no runner contract change.':
        'Existing user-authorized background proof/fuzz workflow preserved. No plan or runner contract change this turn; separate future solver/checker jobs remain unapproved.',
}
for old, new in changes.items():
    assert source.count(old) == 1, old
    source = source.replace(old, new)
exec(compile(source, __file__, 'exec'))
