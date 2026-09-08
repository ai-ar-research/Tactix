#!/usr/bin/env python3
"""Audit explicit non-fuzz authorities for the append checkpoint."""
from pathlib import Path
source=Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start=source.index('jobs={');end=source.index('\ndef sha',start)
source=source[:start]+'''jobs={5434:1669972,5435:1669973,5440:1670118,5441:1670119,
      5447:1670198,5448:1670199,5454:1670292,5455:1670293,
      5458:1670509,5459:None,5462:1670635,5463:1670637,
      5464:1670636,5465:1670638,5466:None,5468:1670699,5469:1670700}'''+source[end:]
source=source.replace('if number in (5437,5449,5453):','if number in (5458,5463):')
source=source.replace('2026-09-07-eliminate-budget-main-ctest.log','2026-09-07-watch-pair-main-ctest.log')
source=source.replace('20260907-2255-eliminate-budget','20260907-2341-watch-pair')
source=source.replace('2026-09-07-eliminate-budget-validation.json','2026-09-07-watch-pair-validation.json')
source=source.replace('seconds=5.32','seconds=5.01')
source=source.replace('Production unchanged. Candidate 5450 passes 81/81 twice and 80/80 trace; causal prefix extends to 7918162 rows; runtime/regression queued; 5445/5450 not covered by ongoing predecessor fuzz.',
    'Production unchanged. Candidate 5466 passes 83/83 twice; 180 actual old/new complete physical-state fixtures identical; exact-work timing and B28+C5 queued; parent bound diagnostics remain pending. Latest 5445/5450/5459/5466 changes not covered by ongoing predecessor fuzz.')
source=source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No manual or reviewer-owned deletion in this checkpoint.',")
exec(compile(source,__file__,'exec'))
