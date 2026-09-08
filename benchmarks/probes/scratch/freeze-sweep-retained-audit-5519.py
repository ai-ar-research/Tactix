#!/usr/bin/env python3
"""Audit named solver campaigns and local validation, excluding owned fuzz."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start = source.index('jobs={')
end = source.index('\ndef sha', start)
source = source[:start] + '''jobs={5465:1670638,5485:1671708,5505:1672156,
      5510:None,5515:1672164,5516:1672165,5517:1672166}''' + source[end:]
source = source.replace("f'20260907-{number}-*'", "f'{20260907 if number == 5465 else 20260908}-{number}-*'")
source = source.replace('if number in (5437,5449,5453):', 'if number in (5505,5515):')
source = source.replace("','.join(str(j) for j in jobs.values() if j)",
                        "','.join([*(str(j) for j in jobs.values() if j), '1672240'])")
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log', '2026-09-08-sweep-retained-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget', '20260908-0252-sweep-retained')
source = source.replace('2026-09-07-eliminate-budget-validation.json', '2026-09-08-sweep-retained-validation.json')
source = source.replace('seconds=5.32', 'seconds=6.35')
start = source.index('    source_status=')
end = source.index('\n    manual_removed_bytes=', start)
source = source[:start] + """    source_status='Production unchanged. Retained-entry5510 on5473: Release85/85 twice, trace84/84, old-binary counterexample. Causal5515 matches11023 native seed/environments;33 skipped one-clause solves expose next gap. B28+C5 5516 pending; timing5517 first pair438.175/410.609, wave1/job1672240 running. Goal unmet.',
    fuzz_owner='5492 resumed Claude session34566 remains live; only terminal response polled. No fuzz jobs/artifacts inspected. It covers5473, not5510.',
""".rstrip() + source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
                        "reviewer_cleanup='No manual deletion. Failed test expectations, pinned inputs/outputs, active campaigns and cited authorities retained.',")
exec(compile(source, __file__, 'exec'))
