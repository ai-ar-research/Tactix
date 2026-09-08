#!/usr/bin/env python3
"""Audit the Sweep candidate and explicitly scoped non-fuzz campaigns."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start = source.index('jobs={')
end = source.index('\ndef sha', start)
source = source[:start] + '''jobs={5447:1670198,5448:1670199,5454:1670292,5455:1670293,
      5458:1670509,5459:None,5462:1670635,5463:1670637,
      5464:1670636,5465:1670638,5466:None,5468:1670699,5469:1670700,
      5470:1670801,5471:1670919,5472:None,5473:None,5475:1671162,5476:1671163}''' + source[end:]
source = source.replace("f'20260907-{number}-*'",
                        "f'{20260908 if number >= 5472 else 20260907}-{number}-*'")
source = source.replace('if number in (5437,5449,5453):',
                        'if number in (5458,5463,5470,5471):')
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log',
                        '2026-09-08-sweep-cursor-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget', '20260908-0020-sweep-cursor')
source = source.replace('2026-09-07-eliminate-budget-validation.json',
                        '2026-09-08-sweep-cursor-validation.json')
source = source.replace('seconds=5.32', 'seconds=5.08')
source = source.replace('Production unchanged. Candidate 5450 passes 81/81 twice and 80/80 trace; causal prefix extends to 7918162 rows; runtime/regression queued; 5445/5450 not covered by ongoing predecessor fuzz.',
    'Production unchanged. Isolated Sweep cursor candidate 5473 passes 84/84 twice, 588 actual old/new fixed ledger cases identical. Exact-work timing 5475 and fixed B28+C5 5476 queued. Latest changes through 5473 remain outside current predecessor-only fuzz. Full goal unmet.')
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No manual campaign deletion. Annotation helper removed only its newly created decompressed profile temporary directory; compressed authority retained.',")
exec(compile(source, __file__, 'exec'))
