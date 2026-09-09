#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('check-vivify-binary-proofs-6272.py')
code=source.read_text()
for a,b in (
    ('20260909-6272-vivify-binary-fixed-proofs','20260909-6310-collapse-fixed-proofs'),
    ('.claude/worktrees/runtime-vivify-binary-retry-6269','.claude/worktrees/runtime-substitute-collapse-6305'),
    ('build/pin-6269-vivify-binary-retry-release','build/pin-6305-substitute-collapse-release'),
    ('test-vivify-binary-proof-6272.cpp','test-collapse-proofs-6310.cpp'),
    ('20260909-6269-vivify-binary-retry-build','20260909-6305-substitute-collapse-build'),
    ('for instantiate in (0, 1):','for instantiate in (1, 3):')):
    assert a in code,a;code=code.replace(a,b)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
