#!/usr/bin/env python3
"""Independent fixed DRAT/LRAT controls; existing formula fixture is unchanged."""
from pathlib import Path
source = Path(__file__).with_name('check-vivify-count-cache-proofs-6386.py')
code = source.read_text()
for before, after in {
    '20260909-6386-vivify-count-cache-proofs': '20260910-6827-vivify-snapshot-proofs',
    '.claude/worktrees/runtime-vivify-count-cache-6383': '.claude/worktrees/runtime-vivify-snapshot-6822',
    'build/pin-6383-vivify-count-cache-release': 'build/pin-6822-vivify-snapshot-release',
    '20260909-6383-vivify-count-cache-build': '20260910-6822-vivify-snapshot-build',
    '432 unit cases separately cover model sets.': '432 baseline plus216 population/rewrite unit cases separately cover model sets.',
}.items():
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
