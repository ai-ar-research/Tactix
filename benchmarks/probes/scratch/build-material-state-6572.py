#!/usr/bin/env python3
"""Three independent approved bookkeeping-only pins, full fixed suites."""
from pathlib import Path
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for old, new in {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-material-state-6571',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-activation-stamp-6424',
    '20260909-6231-substitute-retry-build': '20260909-6571-material-state-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6571-material-state-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n": '',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/kernel/kernel.hpp', 'src/kernel/kernel.cpp', 'src/diagnostics/event_trace.cpp', 'src/rules/analyze/analyze.cpp', 'src/rules/vivify/vivify.cpp', 'tests/test_analyze.cpp', 'tests/test_vivify.cpp', 'tests/test_subsume_material.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Approved separate subsumption-material state and exact producer wiring only. No scheduling consumer. Preserve all shared touch, Factor and removal events; neutral full-work/proof control required before cutover.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        'New actual state tests cover repeats, clear/rearm, fixed additions versus root notices, cold startup, extension, compaction, suppressed and duplicate publication, and public C1/C2 instances. Extended Analyze/Vivify tests cover selective tails and promotion. Existing shared-touch fixture must pass against these objects. User approved the frozen material-tracker design; this is stage one, not promotion or scheduling cutover.',
}.items():
    assert old in code, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
