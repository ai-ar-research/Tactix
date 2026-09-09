#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('build-substitute-retry-6233.py')
code=source.read_text()
for a,b in (
    ('.claude/worktrees/runtime-substitute-retry-6231','.claude/worktrees/runtime-substitute-collapse-6305'),
    ('.claude/worktrees/runtime-learned-sort-6169','.claude/worktrees/runtime-vivify-binary-retry-6269'),
    ('20260909-6231-substitute-retry-build','20260909-6305-substitute-collapse-build'),
    ('build/pin-6231-substitute-retry-','build/pin-6305-substitute-collapse-'),
    ("{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}","{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_collapse.cpp'}"),
    ('Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.',
     'Complete retained-free-endpoint retry advice for canonical irredundant large-to-binary Substitute collapse. Existing primitive only; no shared-kernel, connected Sweep, large, unit, public or proof change.'),
    ('Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.',
     '72 fixed cases cover physical copies, binary/large images, redundancy, canonical/explicit action, proof modes and original-model reconstruction. Expect18 parent-missing cases. Full source suites, independent proofs/reference controls and B/C/full50 gates required.')):
    assert a in code,a;code=code.replace(a,b)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
