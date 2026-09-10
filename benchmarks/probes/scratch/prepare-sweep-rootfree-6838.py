#!/usr/bin/env python3
"""Independent same-work Sweep copy experiment; do not compose unpriced6822."""
from pathlib import Path
import subprocess
subprocess.run(['python3', 'tools/slurm/session_deadline.py', '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
source = Path(__file__).with_name('prepare-collapse-retry-6269.py')
code = source.read_text()
for before, after in {
    '20260909-6231-substitute-retry-build': '20260909-6730-reduce-watermark-build',
    '.claude/worktrees/runtime-vivify-binary-retry-6269': '.claude/worktrees/runtime-sweep-rootfree-6838',
    '20260909-6269-vivify-binary-retry-build': '20260910-6838-sweep-rootfree-build',
}.items():
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
