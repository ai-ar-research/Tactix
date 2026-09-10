#!/usr/bin/env python3
from pathlib import Path
import subprocess
subprocess.run(['python3', 'tools/slurm/session_deadline.py', '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
source = Path(__file__).with_name('submit-compact-stamp-BC-6363.py')
code = source.read_text()
before = '20260909-6362-compact-stamp-BC-gate'
assert code.count(before) == 1
code = code.replace(before, '20260910-6828-vivify-snapshot-BC-gate')
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
