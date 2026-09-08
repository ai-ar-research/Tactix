#!/usr/bin/env python3
"""Check profile neutrality against exact current unprofiled Rank39 arms."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-rephase-profile-5425.py').read_text()
source = source.replace('20260907-5425-rephase-root-profile', '20260908-5544-small-rank39-profile')
source = source.replace('20260907-5421-rephase-root-rank35-runtime', '20260908-5535-sweep-small-ranks1-40')
source = source.replace('5425 profiles pending', '5544 profiles pending')
source = source.replace("r = json.loads(p.read_text()); old[r['task']] = r", "r = json.loads(p.read_text())\n        if r['rank'] == 39: old[r['task']] = r")
source = source.replace('assert len(old) == 9', "if len(old) != 2:\n    print('Unprofiled5535 Rank39 pair pending'); raise SystemExit(0)")
source = source.replace("parents = [v for v in old.values() if v['arm'] == arm]", "parents = [v for v in old.values() if v['arm'] == ('tactix' if arm == 'candidate' else arm)]")
source = source.replace('assert len(parents) == 3 and all(work == counters(v) for v in parents)',
    """assert len(parents) == 1 and work == counters(parents[0])
    assert r['certificate_sha256'] == parents[0]['certificate_sha256']
    assert parents[0]['expected_match'] and not parents[0]['solve']['timed_out']
    if arm == 'candidate':
        stable = lambda row: [s for s in row['solver_statistics'] if not s.startswith('c timing')]
        assert stable(r) == stable(parents[0]), 'Profile changed candidate work'
""".rstrip())
source = source.replace('Both clean full-solve profiles reproduce all three unprofiled work-counter replicas per arm with zero lost samples. Tactix inprocessing dominates the remaining cost investigation.',
    'Both profiles reproduce current unprofiled5535 Rank39 work counters and proof hashes, with complete candidate non-timing statistics equal and zero lost samples. Rank hotspots before selecting an ablation; do not assume inprocessing is the cause.')
exec(compile(source, __file__, 'exec'))
