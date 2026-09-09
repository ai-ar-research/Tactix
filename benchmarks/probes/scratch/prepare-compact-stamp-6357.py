#!/usr/bin/env python3
"""Create an isolated queue-stamp successor without changing any submitted source."""
from pathlib import Path
source = Path(__file__).with_name('prepare-collapse-retry-6269.py')
code = source.read_text()
for before, after in (
    ('20260909-6231-substitute-retry-build', '20260909-6305-substitute-collapse-build'),
    ('.claude/worktrees/runtime-vivify-binary-retry-6269', '.claude/worktrees/runtime-compact-stamp-6357'),
    ('20260909-6269-vivify-binary-retry-build', '20260909-6357-compact-stamp-build'),
):
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
