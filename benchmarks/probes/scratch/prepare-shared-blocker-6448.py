#!/usr/bin/env python3
"""Fresh isolated successor; no mutation of the running activation campaign."""
from pathlib import Path
source=Path(__file__).with_name('prepare-collapse-retry-6269.py')
code=source.read_text()
for old,new in {
 '20260909-6231-substitute-retry-build':'20260909-6424-activation-stamp-build',
 '.claude/worktrees/runtime-vivify-binary-retry-6269':'.claude/worktrees/runtime-shared-blocker-6448',
 '20260909-6269-vivify-binary-retry-build':'20260909-6448-shared-blocker-build',
}.items():
    assert old in code; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
