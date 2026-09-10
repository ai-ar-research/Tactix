#!/usr/bin/env python3
"""Full independent suites for approved BVE mapping/index fusion."""
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
gate = json.loads((root / 'benchmarks/runs/20260910-7110-bve-import-build/focused/summary.json').read_text())
assert gate['status'] == 'PASS'
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
changes = {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-bve-import-7110',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-bve-views-7087',
    '20260909-6231-substitute-retry-build': '20260910-7110-bve-import-build',
    'build/pin-6231-substitute-retry-': 'build/pin-7110-bve-import-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/rules/bve/bve.cpp', 'src/engine/subsolver.cpp', 'src/engine/subsolver.hpp', 'tests/test_subsolver.cpp', 'tests/test_bve_antecedent_view.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Approved BVE preparation stage2: direct mapped physical-clause import into SubSolver, first-occurrence map/touched-list fusion, XOR-local signature indexes. Retain generic add_clause control and physical root-false literals. No kernel primitive, proof vocabulary, public SAT API, scheduling or budget change.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        '252 fixed mapped/copy differential cases with exact ids, ticks, generator, models, core/derivation/refinement; existing brute-force and LRAT replay oracles. Full SubSolver suite ASan/UBSan/LSan PASS. Includes7100 corrected BVE lifecycle fixture. Existing full BVE suite covers all gate families, root-false physical inputs, repeated kernels, one-sided cores and original-model reconstruction. Full source suites here; strong proof and B/C/runtime/fuzz gates still required.'
}
for before, after in changes.items():
    expected = 3 if before == 'build/pin-6231-substitute-retry-' else 1
    assert code.count(before) == expected, (before, code.count(before))
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
