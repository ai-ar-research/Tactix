#!/usr/bin/env python3
"""Audit the isolated repair and its explicit normal jobs; leave fuzz ownership opaque."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start, end = source.index('jobs={'), source.index('\ndef sha')
source = source[:start] + '''jobs={5535:1672328,5550:1672418,5551:None,5592:None,
    5597:1672528,5598:1672529,5599:None,5604:1672600}''' + source[end:]
source = source.replace("f'20260907-{number}-*'", "f'20260908-{number}-*'")
source = source.replace('if number in (5437,5449,5453):', 'if number == 5597:')
source = source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j),'1672596','1672598'])")
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log', '2026-09-08-factor-support-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget', '20260908-0537-factor-support')
source = source.replace('2026-09-07-eliminate-budget-validation.json', '2026-09-08-factor-support-validation.json')
source = source.replace('seconds=5.32', 'seconds=6.26')
start, end = source.index('    source_status='), source.index('\n    manual_removed_bytes=')
source = source[:start] + """    source_status='Production unchanged. Isolated5592 corrects candidate support from root-fixed smaller imported endpoints;87/87 each independent Release,trace86/86,96 actual old/new cases and64 native connection cases.5597 closes all40,102,204 decisions through100K and both Factor19375 outputs. First timing pair323.718/389.784=0.830506 is provisional,not replicated unit-cost evidence. Full5604 runs;goal not met.',
    fuzz_owner='Existing Claude5539 owner70659 remains live and opaque, covers5522 not5592/5546. Only CLI terminal handle polled; no fuzz resources inspected. New candidate needs changed-source coverage before promotion.',
""".rstrip() + source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No manual deletion. A documentation patch mentioning absent isolated docs was rejected atomically, then applied only to owning root docs. Initial TeX overfull lines repaired; final source compiled twice. No candidate test or correctness failure.',")
exec(compile(source, __file__, 'exec'))
