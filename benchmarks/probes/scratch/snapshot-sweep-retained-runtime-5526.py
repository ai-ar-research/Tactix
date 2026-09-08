#!/usr/bin/env python3
"""Score retained-entry semantic repair; require within-arm replica identity."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-watch-runtime-5428.py').read_text()
source = source.replace('20260907-5428-watch-truncate-rank35-runtime', '20260908-5517-sweep-retained-rank32-runtime')
source = source.replace('5428 solver results pending', '5517 solver results pending')
start = source.index('    assert stats(t)==stats(k)')
end = source.index('\n    ts,ks=', start)
source = source[:start] + '''    if wave:
        for offset, row in enumerate((t, k)):
            first = rows[offset]
            assert stats(row) == stats(first), 'Within-arm work changed across replicas'
            assert row['certificate_sha256'] == first['certificate_sha256']
''' .rstrip() + source[end:]
source = source.replace('same_non_timing_statistics_and_proof=True',
                        "same_non_timing_statistics_and_proof=False,within_arm_replica_identity=True")
source = source.replace('Exact-work/proof identity supports a unit-cost comparison, not final correctness before checks or global competitiveness. Candidate remains UNFUZZED and unpromoted.',
                        'Semantic repair changes work/proofs across arms; only within-arm replica identity is required. Paired ratios are net runtime effects, not isolated unit cost. Pending proofs do not establish correctness or promotion; full Rank1-40 and B28+C5 remain separate.')
exec(compile(source, __file__, 'exec'))
