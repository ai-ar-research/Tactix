#!/usr/bin/env python3
"""Run the new fixed fixture against frozen predecessor objects."""
from pathlib import Path
source = Path(__file__).with_name('control-substitute-collapse-6308.py')
code = source.read_text()
for before, after in (
    ('20260909-6308-substitute-collapse-parent-control', '20260909-6359-compact-stamp-parent-control'),
    ('.claude/worktrees/runtime-vivify-binary-retry-6269', '.claude/worktrees/runtime-substitute-collapse-6305'),
    ('.claude/worktrees/runtime-substitute-collapse-6305/tests/test_substitute_collapse.cpp', '.claude/worktrees/runtime-compact-stamp-6357/tests/test_compact_stamp.cpp'),
    ('build/pin-6269-vivify-binary-retry-release', 'build/pin-6305-substitute-collapse-release'),
):
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
