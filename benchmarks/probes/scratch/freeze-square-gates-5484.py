#!/usr/bin/env python3
"""Audit checked gates and explicitly named causal probes, excluding fuzz."""
from pathlib import Path
source = Path(__file__).with_name('freeze-watch-measurement-5481.py').read_text()
source = source.replace('2026-09-08-watch-pair-measured-main-ctest.log',
                        '2026-09-08-square-gates-main-ctest.log')
source = source.replace('20260908-0047-watch-pair', '20260908-0107-square-gates')
source = source.replace('2026-09-08-watch-pair-measured-validation.json',
                        '2026-09-08-square-gates-validation.json')
source = source.replace('seconds=5.14', 'seconds=20.76')
source = source.replace('No solver implementation change. Watch-pair 5468 has all six expected solver verdicts with complete exact-statistics/proof-byte pairs, median 0.983980273x predecessor, range 0.981238269-0.993027626, both arm spreads below 2%. Proof checks pending, no promotion. Full goal remains unmet at latest complete 1.173817099x; later causal traces pending. Latest 5473 remains isolated/unfuzzed.',
    'No solver implementation change. Gates 5455 and 5464 are each 66 checked/clean. Rank31 has 51491857 matching decisions and 276 boundaries through 100K. Square first splits on polarity at conflict501/decision5849; selected phase replay5482 submitted. Full goal unmet at 1.173817099x; latest5473 isolated/unfuzzed.')
anchor = "exec(compile(source, __file__, 'exec'))"
assert source.count(anchor) == 1
source = source.replace(anchor, """source = source.replace('5479:1671263}', '5479:1671263,5482:1671600}')
source = source.replace('(5458,5463,5470,5471,5478,5479):', '(5458,5463,5470,5471,5478,5479,5482):')
""" + anchor)
exec(compile(source, __file__, 'exec'))
