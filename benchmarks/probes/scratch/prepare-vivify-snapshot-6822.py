#!/usr/bin/env python3
"""Create a private same-work Vivify representation experiment from immutable6730."""
from pathlib import Path
import subprocess
subprocess.run(['python3', 'tools/slurm/session_deadline.py', '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
source = Path(__file__).with_name('prepare-collapse-retry-6269.py')
code = source.read_text()
for before, after in {
    '20260909-6231-substitute-retry-build': '20260909-6730-reduce-watermark-build',
    '.claude/worktrees/runtime-vivify-binary-retry-6269': '.claude/worktrees/runtime-vivify-snapshot-6822',
    '20260909-6269-vivify-binary-retry-build': '20260910-6822-vivify-snapshot-build',
}.items():
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
