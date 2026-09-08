#!/usr/bin/env python3
"""Audit only named normal campaigns; Claude's fuzz resources remain opaque."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start = source.index('jobs={')
end = source.index('\ndef sha', start)
source = source[:start] + '''jobs={5485:1671708,5516:1672165,5517:1672166,
      5522:None,5529:1672251,5530:1672252,5531:1672253,
      5535:1672328,5537:1672356}''' + source[end:]
source = source.replace("f'20260907-{number}-*'", "f'20260908-{number}-*'")
source = source.replace('if number in (5437,5449,5453):', 'if number in (5529,5537):')
source = source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j), '1672240', '1672244', '1672326', '1672385'])")
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log', '2026-09-08-sweep-small-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget', '20260908-0335-sweep-small')
source = source.replace('2026-09-07-eliminate-budget-validation.json', '2026-09-08-sweep-small-validation.json')
source = source.replace('seconds=5.32', 'seconds=5.70')
start = source.index('    source_status=')
end = source.index('\n    manual_removed_bytes=', start)
source = source[:start] + """    source_status='Production unchanged.5522 real small-environment Sweep solves:Release86/86 twice,trace85/85,12 native engine cases.5529 closes18593182 Rank32 decisions.5531 first two pairs improve,final wave1672385 runs; full5535 and extended5537 run. Goal unmet.',
    fuzz_owner='Claude terminal5492 PASS on5473:60000 cases,55036 checked verdicts,zero failures,360 reachability cases. New5539 owner/session92433 tests5522; only terminal response may be polled. No fuzz artifacts/jobs inspected.',
""".rstrip() + source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No manual deletion. Failed fixture expectation logs, source pins, causal evidence, pending campaigns and cited authorities retained.',")
exec(compile(source, __file__, 'exec'))
