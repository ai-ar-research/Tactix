#!/usr/bin/env python3
"""Freeze a scoped normal-campaign audit without inspecting either fuzz owner."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start = source.index('jobs={')
end = source.index('\ndef sha', start)
source = source[:start] + '''jobs={5447:1670198,5448:1670199,5454:1670292,5455:1670293,
      5458:1670509,5459:None,5462:1670635,5463:1670637,5464:1670636,
      5465:1670638,5466:None,5468:1670699,5469:1670700,5470:1670801,
      5471:1670919,5472:None,5473:None,5475:1671162,5476:1671163,
      5478:1671258,5479:1671263,5482:1671600,5485:1671708,5487:1671745,
      5490:1671777}''' + source[end:]
source = source.replace("f'20260907-{number}-*'", "f'{20260908 if number >= 5472 else 20260907}-{number}-*'")
source = source.replace('if number in (5437,5449,5453):',
                        'if number in (5458,5463,5470,5471,5478,5479,5482,5487,5490):')
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log', '2026-09-08-sweep-causal-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget', '20260908-0124-sweep-causal')
source = source.replace('2026-09-07-eliminate-budget-validation.json', '2026-09-08-sweep-causal-validation.json')
source = source.replace('seconds=5.32', 'seconds=6.00')
start = source.index('    source_status=')
end = source.index('\n    manual_removed_bytes=', start)
source = source[:start] + '''    source_status='Production unchanged. Sweep5475 exact-work median0.984562853; full5485 submitted. Square saved-phase history localized with eight normalization fixtures; Rank32 Sweep and later Rank37 boundaries localized. New selected probes pending. Goal unmet.',
    fuzz_owner='5442 terminal PASS on5438 accepted without artifact inspection. New5492 owner process3041017/session9171 pending on latest5473; terminal response only.',
''' .rstrip() + source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
                        "reviewer_cleanup='No manual data deletion in this checkpoint.',")
exec(compile(source, __file__, 'exec'))
