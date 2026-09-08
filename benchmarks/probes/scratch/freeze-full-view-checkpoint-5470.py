#!/usr/bin/env python3
"""Audit the complete scoring view and explicit non-fuzz successors."""
from pathlib import Path
source=Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start=source.index('jobs={');end=source.index('\ndef sha',start)
source=source[:start]+'''jobs={5434:1669972,5435:1669973,5440:1670118,5441:1670119,
      5447:1670198,5448:1670199,5454:1670292,5455:1670293,
      5458:1670509,5459:None,5462:1670635,5463:1670637,
      5464:1670636,5465:1670638,5466:None,5468:1670699,5469:1670700,
      5470:1670801}'''+source[end:]
source=source.replace('if number in (5437,5449,5453):','if number in (5458,5463,5470):')
source=source.replace('2026-09-07-eliminate-budget-main-ctest.log','2026-09-07-phase-budget-full-main-ctest.log')
source=source.replace('20260907-2255-eliminate-budget','20260907-2350-phase-budget-full')
source=source.replace('2026-09-07-eliminate-budget-validation.json','2026-09-07-phase-budget-full-validation.json')
source=source.replace('seconds=5.32','seconds=5.16')
source=source.replace('Production unchanged. Candidate 5450 passes 81/81 twice and 80/80 trace; causal prefix extends to 7918162 rows; runtime/regression queued; 5445/5450 not covered by ongoing predecessor fuzz.',
    'No solver implementation changed. Complete 5454 solver view is 1.170282883x, 63/80 checked at snapshot. 5434 full, 5440 timing and 5441 B28+C5 now checked. Rank 31 selected for new causal probe 5470. Latest 5466 remains isolated/unfuzzed.')
source=source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No manual or reviewer-owned deletion in this checkpoint.',")
exec(compile(source,__file__,'exec'))
