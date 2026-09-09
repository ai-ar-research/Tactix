#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('control-substitute-retry-6232.py')
code = source.read_text()
changes = {
    '.claude/worktrees/runtime-substitute-retry-6231': '.claude/worktrees/runtime-vivify-binary-retry-6269',
    '20260909-6231-substitute-retry-build': '20260909-6269-vivify-binary-retry-build',
    'build/pin-6169-learned-sort-release': 'build/pin-6231-substitute-retry-release',
    'test_substitute_retry.cpp': 'test_vivify_binary_retry.cpp',
}
for a,b in changes.items(): assert a in code; code = code.replace(a,b)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
