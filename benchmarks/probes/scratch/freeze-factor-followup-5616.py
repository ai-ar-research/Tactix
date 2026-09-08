#!/usr/bin/env python3
"""Freeze explicit normal gates and the completed score, leaving fuzz ownership opaque."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start, end = source.index('jobs={'), source.index('\ndef sha')
source = source[:start] + '''jobs={5535:1672328,5550:1672418,5551:None,5592:None,
    5597:1672528,5598:1672529,5599:None,5604:1672600,5611:1672685,5613:1672687}''' + source[end:]
source = source.replace("f'20260907-{number}-*'", "f'20260908-{number}-*'")
source = source.replace('if number in (5437,5449,5453):', 'if number in (5597,5611,5613):')
source = source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j),'1672596','1672598','1672681'])")
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log', '2026-09-08-factor-followup-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget', '20260908-0603-factor-full')
source = source.replace('2026-09-07-eliminate-budget-validation.json', '2026-09-08-factor-followup-validation.json')
source = source.replace('seconds=5.32', 'seconds=12.10')
start, end = source.index('    source_status='), source.index('\n    manual_removed_bytes=')
source = source[:start] + """    source_status='Production unchanged;repair5592 checkpointed inb15b114e. Full5604 all80 solves,58 checked at frozen snapshot,1.13758865xKissat;goal not met. Three5599 pairs median0.824875x5522,within-arm work/proof identical,spreads1.60%/0.52%;proofs pending. Current Congruence probes5611/5613 queued/running,not a new solver repair.',
    fuzz_owner='Claude terminal5539 PASS on5522:70,000 cases,64,364 checked verdicts,zero failures;70659 closed. New5610/session78639 owns changed5592. Terminal responses/CLI handle only; no fuzz job or artifact inspection or independent reinterpretation.',
""".rstrip() + source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No parent manual deletion. Claude terminal5539 reports its campaign-owned cleanup. New16 clause-state parser fixtures pass;existing27 parser/selector fixtures pass;main77/77. Frozen raw logs/patches/CRLF matrices preserve original bytes; hand-edited files pass whitespace checks.',")
exec(compile(source, __file__, 'exec'))
