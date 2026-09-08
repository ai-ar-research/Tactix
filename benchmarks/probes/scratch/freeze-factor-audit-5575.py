#!/usr/bin/env python3
"""Freeze explicit current normal-job evidence; never inspect fuzz resources."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start,end = source.index('jobs={'),source.index('\ndef sha')
source = source[:start] + '''jobs={5530:1672252,5531:1672253,5535:1672328,
      5544:1672400,5550:1672418,5551:None,5558:1672426,
      5564:1672430,5565:1672432,5567:1672434,5569:1672436,5571:1672476}''' + source[end:]
source = source.replace("f'20260907-{number}-*'", "f'20260908-{number}-*'")
source = source.replace('if number in (5437,5449,5453):',
                        'if number in (5558,5564,5565,5567,5569,5571):')
source = source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j),'1672326','1672385'])")
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log','2026-09-08-factor-input-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget','20260908-0436-factor-input')
source = source.replace('2026-09-07-eliminate-budget-validation.json','2026-09-08-factor-input-validation.json')
source = source.replace('seconds=5.32','seconds=6.06')
start = source.index('    source_status=')
end = source.index('\n    manual_removed_bytes=',start)
source = source[:start] + """    source_status='Production unchanged. Diagnostic-only increment:5544 checked work/proof-neutral profiles; Rank33 next decision split182093; Rank32 equal binary multisets and large order, only two literal poses differ, first grid split4825. No new runtime gain; full5535 remains1.1621795918x. Goal unmet.',
    fuzz_owner='Same Claude5539 conversation under live70659 covers5522,not5546. Terminal response only; no fuzz resource/artifact inspection or duplicate campaign.',
""".rstrip() + source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No manual deletion. Original5566 rejected extra broad-observer journal rows; successor5574 verifies original ordered rows and enumerates additions. Native5558 trace staging corrected only after completed result/hash validation.',")
exec(compile(source,__file__,'exec'))
