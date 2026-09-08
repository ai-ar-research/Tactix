#!/usr/bin/env python3
"""Audit normal solver campaigns and frozen optimization evidence, not fuzz data."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start = source.index('jobs={')
end = source.index('\ndef sha', start)
source = source[:start] + '''jobs={5485:1671708,5516:1672165,5517:1672166,
      5530:1672252,5531:1672253,5535:1672328,5537:1672356,
      5544:1672400,5546:None,5550:1672418,5551:None,5558:1672426}''' + source[end:]
source = source.replace("f'20260907-{number}-*'", "f'20260908-{number}-*'")
source = source.replace('if number in (5437,5449,5453):', 'if number in (5537,5558):')
source = source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j), '1672240', '1672244', '1672326', '1672385'])")
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log', '2026-09-08-vivify-census-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget', '20260908-0400-vivify-census')
source = source.replace('2026-09-07-eliminate-budget-validation.json', '2026-09-08-vivify-census-validation.json')
source = source.replace('seconds=5.32', 'seconds=5.80')
start = source.index('    source_status=')
end = source.index('\n    manual_removed_bytes=', start)
source = source[:start] + """    source_status='Production unchanged.5546 skips unused Vivify census;87/87 twice,192 actual old/new schedule/state/proof cases.5550 B28+C5 runs;5551 timing prepared pending B28 snapshots. Parent5535 full1.1621795918x,all80 solves; next losses37/33.5537 next split20095,earlier Factor13-clause count gap. Goal unmet.',
    fuzz_owner='5539 initial owner92433 returned submission-only; same conversation resumed under70659, live. Covers5522,not5546. Parent polls terminal response only, no fuzz jobs/artifacts inspected. Prior5492 PASS on5473 retained.',
""".rstrip() + source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No manual deletion. Unpublished copy scaffold resumed only after exact hash/empty-directory checks. Immutable sources, failed observer expectation, profiles and pending campaigns retained.',")
exec(compile(source, __file__, 'exec'))
