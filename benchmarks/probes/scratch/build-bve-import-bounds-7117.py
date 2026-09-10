#!/usr/bin/env python3
"""Full independent builds for the warning-closure successor of7110."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
m = json.loads((root / 'benchmarks/runs/20260910-7110-bve-import-build/manifest.json').read_text())
candidate = root / '.claude/worktrees/runtime-bve-import-bounds-7116'
for rel in ('src/engine/subsolver.cpp', 'src/engine/subsolver.hpp', 'tests/test_subsolver.cpp'):
    assert sha(candidate / rel) == m['files'][rel], rel
source = Path(__file__).with_name('build-bve-import-7112.py')
code = source.read_text()
code = code.replace('7110-bve-import', '7116-bve-import-bounds')
code = code.replace('runtime-bve-import-7110', 'runtime-bve-import-bounds-7116')
code = code.replace('20260910-7116-bve-import-bounds-build/focused/summary.json',
                    '20260910-7110-bve-import-build/focused/summary.json')
before = "'tests/test_subsolver.cpp', 'tests/test_bve_antecedent_view.cpp'}"
assert code.count(before) == 1
code = code.replace(before, "'tests/test_subsolver.cpp', 'tests/test_bve_antecedent_view.cpp', 'tests/test_bve.cpp'}")
code = code.replace('Includes7100 corrected BVE lifecycle fixture.',
    'Includes7100 corrected BVE lifecycle fixture with explicit vector snapshot; boundary XOR fixtures for all supported arities and the first unsupported arity. Consumer reasserts the signature builder bound for GCC range analysis. Unchanged SubSolver source retains7111 instrumented coverage.')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
