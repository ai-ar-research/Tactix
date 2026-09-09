#!/usr/bin/env python3
"""Clone a private successor, leaving every submitted source/pin immutable."""
from pathlib import Path
source = Path(__file__).with_name('prepare-collapse-retry-6269.py')
code = source.read_text()
for old, new in {
    '20260909-6231-substitute-retry-build': '20260909-6357-compact-stamp-build',
    '.claude/worktrees/runtime-vivify-binary-retry-6269': '.claude/worktrees/runtime-vivify-count-cache-6383',
    '20260909-6269-vivify-binary-retry-build': '20260909-6383-vivify-count-cache-build',
}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
