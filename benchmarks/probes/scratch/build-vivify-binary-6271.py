#!/usr/bin/env python3
"""Independent full suites and immutable pins for binary-Vivify advice."""
from pathlib import Path
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
changes = {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-vivify-binary-retry-6269',
    '.claude/worktrees/runtime-learned-sort-6169': '.claude/worktrees/runtime-substitute-retry-6231',
    '20260909-6231-substitute-retry-build': '20260909-6269-vivify-binary-retry-build',
    'build/pin-6231-substitute-retry-': 'build/pin-6269-vivify-binary-retry-',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/rules/vivify/vivify.cpp', 'tests/test_vivify_binary_retry.cpp'}",
    "hypothesis='Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.'":
        "hypothesis='Complete binary-Vivify original-endpoint retry advice, including instantiated binary terminals. No shared-kernel or other-rule change. N1/proof/reconstruction unchanged.'",
    "test_consideration='Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.'":
        "test_consideration='Actual frozen parent lacks all24 irredundant cases of48 public-instance fixtures covering proof modes, learned/instantiated binaries, reuse and preset flags; redundant cases remain neutral. Full suites, direct proof/model checks, native-reference evidence and gates required.'",
}
for a,b in changes.items(): assert a in code, a; code = code.replace(a,b)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
