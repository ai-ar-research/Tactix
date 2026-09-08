#!/usr/bin/env python3
"""Audit the short witnesses and explicitly scoped non-fuzz successors."""
from pathlib import Path
source = Path(__file__).with_name('freeze-full-view-checkpoint-5470.py').read_text()
source = source.replace('5470:1670801}', '5470:1670801,5471:1670919}')
source = source.replace('(5458,5463,5470):', '(5458,5463,5470,5471):')
source = source.replace('2026-09-07-phase-budget-full-main-ctest.log',
                        '2026-09-08-short-bound-main-ctest.log')
source = source.replace('20260907-2350-phase-budget-full',
                        '20260908-0000-short-bound')
source = source.replace('2026-09-07-phase-budget-full-validation.json',
                        '2026-09-08-short-bound-validation.json')
source = source.replace('seconds=5.16', 'seconds=20.49')
source = source.replace('No solver implementation changed. Complete 5454 solver view is 1.170282883x, 63/80 checked at snapshot. 5434 full, 5440 timing and 5441 B28+C5 now checked. Rank 31 selected for new causal probe 5470. Latest 5466 remains isolated/unfuzzed.',
                        'No solver implementation changed. Native bound cut confirmed; nine short controls checked; Square causal probe queued; cursor six proofs checked. Bound prefix 5463 is terminal and compared. Latest 5466 remains isolated/unfuzzed; full goal unmet.')
exec(compile(source, __file__, 'exec'))
