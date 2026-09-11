#!/usr/bin/env python3
"""Full optimized builds and actual Debug-sanitized inherited-failure gates."""
from pathlib import Path
base = Path(__file__).with_name('build-root-fold-7332.py')
code = base.read_text()
changes = {
    'runtime-root-fold-7330': 'runtime-debug-contracts-7339',
    '20260911-7332-root-fold-build': '20260911-7340-debug-contracts-build',
    'build/pin-7332-root-fold-': 'build/pin-7340-debug-contracts-',
    "fixtures = ('root_fold', 'compact_stamp'": "fixtures = ('root_fold', 'root_watermark', 'collect_connection', 'forward_unit', 'substitute_collapse', 'substitute_retry', 'subsume_material', 'compact_stamp'",
    "count = 120 if kind == 'trace' else 121": "count = 122 if kind == 'trace' else 123",
    "100% tests passed, 0 tests failed out of {count}": "100% tests passed out of {count}",
    "max_workers=3": "max_workers=4",
    "('bc', 'trace', 'sanitizers')": "('release', 'bc', 'trace', 'sanitizers')",
    'build/pin-7330-root-fold-release/tactix': 'build/pin-7340-debug-contracts-release/tactix',
    'eight fixed': 'fourteen fixed',
    'original-CNF 7331 microchecks': 'original-CNF successor microchecks',
    'for p in [Path(__file__), release,': 'for p in [Path(__file__), Path(__file__).with_name("build-root-fold-7332.py"), release,',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
