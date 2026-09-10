#!/usr/bin/env python3
"""Compare actual parent/current Vivify objects on state, order and proof fixtures."""
from pathlib import Path
source = Path(__file__).with_name('control-vivify-count-cache-6384.py')
code = source.read_text()
for before, after in {
    '20260909-6383-vivify-count-cache-build': '20260910-6822-vivify-snapshot-build',
    'control-6384.json': 'control-6823.json',
    '.claude/worktrees/runtime-vivify-count-cache-6383': '.claude/worktrees/runtime-vivify-snapshot-6822',
    '.claude/worktrees/runtime-compact-stamp-6357': '.claude/worktrees/runtime-reduce-watermark-6730',
    'build/pin-6357-compact-stamp-release': 'build/pin-6730-reduce-watermark-release',
    'test_vivify_count_cache': 'test_vivify_snapshot',
}.items():
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
