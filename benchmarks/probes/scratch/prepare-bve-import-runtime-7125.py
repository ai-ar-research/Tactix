#!/usr/bin/env python3
"""Predeclare the full approved BVE prototype versus immutable7030."""
from pathlib import Path
source = Path(__file__).with_name('prepare-bve-views-runtime-7105.py')
code = source.read_text()
changes = {
    '20260910-7105-bve-views-runtime': '20260910-7125-bve-import-runtime',
    '20260910-7103-bve-views-BC-gate': '20260910-7122-bve-import-BC-gate',
    'neutrality-7104.json': 'neutrality-7123.json',
    '7087-bve-views': '7116-bve-import-bounds',
    "'7103-bve-views-BC', '7105-bve-views-runtime'": "'7122-bve-import-BC', '7125-bve-import-runtime'",
    '20260910-7101-bve-view-proofs': '20260910-7119-bve-import-bounds-proofs',
    '20260910-7100-bve-fixture-repair': '20260910-7118-bve-import-bounds-sanitizers',
    'tactix-bve-views-three-wave-runtime-v1': 'tactix-bve-import-three-wave-runtime-v1',
    'Borrowing root-free BVE antecedents avoids literal copies while retaining logical work and proof order. Measure full-solver/printed-stage time, RSS and transfer effects; no scheduling or budget change. Whole-Eliminate20% target belongs to the complete multi-stage prototype, not this initial view stage.':
        'Complete approved BVE preparation prototype: stable antecedent views, direct physical-clause mapped import, fused variable-map/touched list and consumer-local XOR indexes. Require exact work/proofs and no whole-solver/RSS regression. Target20% complete-Eliminate reduction on35, assessed by separate matched full-lifecycle profiles rather than the printed BVE timer; do not lower the target if missed.',
    'Original7087 full114/114/113 suites; direct7100 Debug fixture repair and ASan/UBSan/LSan passes;96 strict7101 proofs and98 negatives;66 B/C exact-work outcomes. Runtime runner unchanged. Existing fingerprint fixtures cover timing exclusion, work/certificate changes and malformed statistics; enforce18-task/three-rank topology and all cross-arm identities.':
        '7116 full114/114/113 suites;252 mapped/copy cases and full SubSolver ASan/UBSan/LSan;496 XOR boundary/model cases and full Debug BVE/view/memory sanitizers;96 definition and120 Sweep strict proofs with220 negatives, exact7087;B/C66 exact outcomes. Runtime runner unchanged. Existing fingerprint/timer tests cover18 tasks, reversed middle wave, work/certificate differences and malformed statistics.',
}
for i, (before, after) in enumerate(changes.items()):
    assert before in code, before
    code = code.replace(before, f'__STAGE2_{i}__')
for i, after in enumerate(changes.values()): code = code.replace(f'__STAGE2_{i}__', after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
