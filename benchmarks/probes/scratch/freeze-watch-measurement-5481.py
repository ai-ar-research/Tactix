#!/usr/bin/env python3
"""Audit the complete append timing measurement and existing normal campaigns."""
from pathlib import Path
source = Path(__file__).with_name('freeze-bound-view-checkpoint-5480.py').read_text()
source = source.replace('2026-09-08-bound-full-main-ctest.log',
                        '2026-09-08-watch-pair-measured-main-ctest.log')
source = source.replace('20260908-0030-bound-full', '20260908-0047-watch-pair')
source = source.replace('2026-09-08-bound-full-validation.json',
                        '2026-09-08-watch-pair-measured-validation.json')
source = source.replace('seconds=5.03', 'seconds=5.14')
source = source.replace('No solver implementation change. All 80 bound solver records give 1.173817099x Kissat, 58/80 checked at snapshot, no verdict loss or timeout. Rank-37 full work regresses after its proved 100000-conflict common prefix; longer Rank-37 and independent Rank-32 traces submitted. Latest 5473 isolated/unfuzzed. Full goal unmet.',
    'No solver implementation change. Watch-pair 5468 has all six expected solver verdicts with complete exact-statistics/proof-byte pairs, median 0.983980273x predecessor, range 0.981238269-0.993027626, both arm spreads below 2%. Proof checks pending, no promotion. Full goal remains unmet at latest complete 1.173817099x; later causal traces pending. Latest 5473 remains isolated/unfuzzed.')
exec(compile(source, __file__, 'exec'))
