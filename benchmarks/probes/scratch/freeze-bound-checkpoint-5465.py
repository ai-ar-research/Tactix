#!/usr/bin/env python3
"""Reuse the scoped checkpoint audit without inspecting fuzz-owned resources."""
from pathlib import Path
source=Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start=source.index('jobs={');end=source.index('\ndef sha',start)
source=source[:start]+'''jobs={5434:1669972,5435:1669973,5440:1670118,5441:1670119,
      5447:1670198,5448:1670199,5454:1670292,5455:1670293,5456:None,
      5457:1670359,5458:1670509,5459:None,5462:1670635,5463:1670637,
      5464:1670636,5465:1670638}'''+source[end:]
source=source.replace('if number in (5437,5449,5453):','if number in (5457,5458,5463):')
needle="    for rel,digest in m.get('binaries',{}).items():assert sha(root/rel)==digest"
source=source.replace(needle,needle+'''
    for entry in m.get('sources',{}).values():
        for rel,digest in entry.get('files',{}).items():assert sha(root/entry['source']/rel)==digest''')
source=source.replace('2026-09-07-eliminate-budget-main-ctest.log','2026-09-07-bound-lifecycle-main-ctest.log')
source=source.replace('20260907-2255-eliminate-budget','20260907-2332-bound-lifecycle')
source=source.replace('2026-09-07-eliminate-budget-validation.json','2026-09-07-bound-lifecycle-validation.json')
source=source.replace('seconds=5.32','seconds=5.19')
source=source.replace('Production unchanged. Candidate 5450 passes 81/81 twice and 80/80 trace; causal prefix extends to 7918162 rows; runtime/regression queued; 5445/5450 not covered by ongoing predecessor fuzz.',
    'Production unchanged. Candidate 5459 passes 82/82 twice and 81/81 trace; restores documented bound lifecycle after quota cuts; short witnesses, causal prefix, runtime/regression queued; 5445/5450/5459 not covered by ongoing predecessor fuzz.')
source=source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No manual cleanup or reviewer-owned deletion in this checkpoint.',")
exec(compile(source,__file__,'exec'))
