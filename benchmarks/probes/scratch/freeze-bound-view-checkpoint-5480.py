#!/usr/bin/env python3
"""Audit complete bound scoring and the explicit non-fuzz diagnostic successors."""
from pathlib import Path
source = Path(__file__).with_name('freeze-sweep-checkpoint-5476.py').read_text()
source = source.replace('5476:1671163}', '5476:1671163,5478:1671258,5479:1671263}')
source = source.replace('(5458,5463,5470,5471):', '(5458,5463,5470,5471,5478,5479):')
source = source.replace('2026-09-08-sweep-cursor-main-ctest.log',
                        '2026-09-08-bound-full-main-ctest.log')
source = source.replace('20260908-0020-sweep-cursor', '20260908-0030-bound-full')
source = source.replace('2026-09-08-sweep-cursor-validation.json',
                        '2026-09-08-bound-full-validation.json')
source = source.replace('seconds=5.08', 'seconds=5.03')
source = source.replace('Production unchanged. Isolated Sweep cursor candidate 5473 passes 84/84 twice, 588 actual old/new fixed ledger cases identical. Exact-work timing 5475 and fixed B28+C5 5476 queued. Latest changes through 5473 remain outside current predecessor-only fuzz. Full goal unmet.',
    'No solver implementation change. All 80 bound solver records give 1.173817099x Kissat, 58/80 checked at snapshot, no verdict loss or timeout. Rank-37 full work regresses after its proved 100000-conflict common prefix; longer Rank-37 and independent Rank-32 traces submitted. Latest 5473 isolated/unfuzzed. Full goal unmet.')
source = source.replace('No manual campaign deletion. Annotation helper removed only its newly created decompressed profile temporary directory; compressed authority retained.',
                        'No manual or reviewer-owned data deletion in this checkpoint.')
exec(compile(source, __file__, 'exec'))
