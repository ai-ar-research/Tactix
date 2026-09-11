#!/usr/bin/env python3
"""Validate the successor; preserve 7332 parser/LSan-environment failures."""
from pathlib import Path
base = Path(__file__).with_name('build-root-fold-7332.py')
code = base.read_text()
changes = {
    'runtime-root-fold-7330': 'runtime-root-watermark-7333',
    '20260911-7332-root-fold-build': '20260911-7335-root-watermark-build',
    'build/pin-7332-root-fold-': 'build/pin-7335-root-watermark-',
    "fixtures = ('root_fold', 'compact_stamp'": "fixtures = ('root_fold', 'root_watermark', 'compact_stamp'",
    "count = 120 if kind == 'trace' else 121": "count = 121 if kind == 'trace' else 122",
    "100% tests passed, 0 tests failed out of {count}": "100% tests passed out of {count}",
    "max_workers=3": "max_workers=4",
    "('bc', 'trace', 'sanitizers')": "('release', 'bc', 'trace', 'sanitizers')",
    'build/pin-7330-root-fold-release/tactix': 'build/pin-7335-root-watermark-release/tactix',
    'eight fixed': 'nine fixed',
    'original-CNF 7331 microchecks': 'original-CNF successor microchecks',
    'for p in [Path(__file__), release,': 'for p in [Path(__file__), Path(__file__).with_name("build-root-fold-7332.py"), release,',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
