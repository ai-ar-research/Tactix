#!/usr/bin/env python3
"""Three independent approved C2/Eliminate cutover pins, full fixed suites."""
from pathlib import Path
import sys
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for old, new in {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-material-schedule-6579',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-material-state-6571',
    '20260909-6231-substitute-retry-build': '20260909-6579-material-schedule-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6579-material-schedule-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/rules/strengthen/strengthen.cpp', 'src/policies/default_policy.cpp', 'src/policies/default_inprocess.hpp', 'tests/test_strengthen.cpp', 'tests/test_forward_marks.cpp', 'tests/test_scheduling.cpp', 'tests/test_bve_bound_cut.cpp', 'tests/test_subsume_material.cpp', 'tests/test_material_schedule.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Approved canonical C2 persistent-flag predicates and clear/rearm lifecycle plus Eliminate cumulative material admission and maximum-bound exit watermark. Preserve all shared producers, explicit band, public instances and SP-SLOT-5. Recover missing Rank50 material invitation without benchmark-specific policy.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        'Actual new consumer tests cover all three predicates versus shared advice, complete clear, budget-tail/large/binary rearm without recount, explicit band isolation, interruption and C1 independence, material-only/deadline/root/removal admission and non-owning phase entry. Extended tests cover partial interruption and max/lower-bound exit. Existing producer and public-instance tests remain. Full B/C and focused proof/model gates precede targeted Rank50; promotion remains blocked by missing fuzz and aggregate/memory gates.',
}.items():
    assert old in code, old
    code = code.replace(old, new)
if sys.argv[1:] == ['--resume-fixture']:
    # Initial builds completed, but the new fixture failed on two incorrect
    # assumptions. Keep first logs; no source manifest or job owns these pins.
    assert Path('benchmarks/probes/scratch/material-schedule-fixture-failure-6583.md').is_file()
    code = code.replace('assert not out.exists()', 'assert out.is_dir()')
    code = code.replace("    run(kind + '-configure', command)\n", '')
    code = code.replace("run(kind + '-build',", "run(kind + '-build-2',")
    code = code.replace("run(kind + '-ctest',", "run(kind + '-ctest-2',")
else:
    assert len(sys.argv) == 1
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
