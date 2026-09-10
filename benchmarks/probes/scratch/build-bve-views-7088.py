#!/usr/bin/env python3
"""Full independent suites for approved BVE views stage1; do not rerun."""
from pathlib import Path
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
changes = {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-bve-views-7087',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-collect-retry-7030',
    '20260909-6231-substitute-retry-build': '20260910-7087-bve-views-build',
    'build/pin-6231-substitute-retry-': 'build/pin-7087-bve-views-',
    '    assert not out.exists()': '    assert kind == "release" or not out.exists()',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/rules/bve/bve.cpp', 'src/rules/bve/antecedent.hpp', 'tests/test_bve_antecedent_view.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Approved BVE data-path stage1: borrow stable arena-offset slices for root-free large antecedents; preserve pooled root-filtered snapshots and LRAT hints. Consumers/order/ticks/definition input/proofs/model/N1 unchanged; no kernel layout or scheduling change.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        '162 actual preparation poses in both proof modes; explicit zero-copy owner assertion, pool/arena growth, late roots, physical duplicates/garbage; sparse+dense public BVE forces arena reallocation before witnesses and reconstructs original clauses. Existing whole BVE suite covers gate families and70754 models. Full three suites, strict certificates and runtime controls remain required. User explicitly approved after73a78fd8; no fuzz/promotion authority.'
}
for before, after in changes.items():
    expected = 3 if before == 'build/pin-6231-substitute-retry-' else 1
    assert code.count(before) == expected, (before, code.count(before))
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
