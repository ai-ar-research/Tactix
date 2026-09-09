#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('prepare-collapse-retry-6269.py')
code=source.read_text()
for a,b in (('20260909-6231-substitute-retry-build','20260909-6269-vivify-binary-retry-build'),
            ('.claude/worktrees/runtime-vivify-binary-retry-6269','.claude/worktrees/runtime-substitute-collapse-6305'),
            ('20260909-6269-vivify-binary-retry-build\'\nassert','20260909-6305-substitute-collapse-build\'\nassert')):
    assert a in code,a;code=code.replace(a,b)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
