#!/usr/bin/env python3
"""Reuse the retained checkpoint audit for the new evidence and job mapping."""
from pathlib import Path

directory = Path(__file__).resolve().parent
source = (directory / 'freeze-root-followup-5435.py').read_text()
changes = {
    '5433:1669971,5434:1669972,5435:1669973}':
        '5433:1669971,5434:1669972,5435:1669973,5436:1669994,5437:1670050}',
    'if number in (5424,5433):': 'if number in (5424,5433,5436,5437):',
    '2026-09-07-root-entry-main-ctest.log': '2026-09-07-runtime-boundaries-main-ctest.log',
    '2026-09-07-root-entry-validation.json': '2026-09-07-runtime-boundaries-validation.json',
    '20260907-2055-root-entry': '20260907-2115-runtime-boundaries',
    'seconds=6.38': 'seconds=5.90',
    'Incomplete views do not establish an aggregate or acceptance; pending expected results guide dependent work.':
        'Only all forty complete pairs establish the provisional aggregate. Pending expected results guide dependent work, not final acceptance.',
    'Production unchanged; independent metadata 5426 and semantic root-entry 5430 remain UNFUZZED and unpromoted.':
        'Production unchanged. Rephase full solver view 1.202034x, timing and BC final; metadata small exact-work gain measured. Isolated candidates remain UNFUZZED and unpromoted.'}
for old, new in changes.items():
    assert source.count(old) == 1, old
    source = source.replace(old, new)
exec(compile(source, __file__, 'exec'))
