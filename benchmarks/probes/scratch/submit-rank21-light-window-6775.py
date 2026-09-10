#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
source = Path(__file__).with_name('submit-material-rank31-6606.py')
code = source.read_text()
old = '20260909-6605-material-rank31-prefix'
assert code.count(old) == 1
exec(compile(code.replace(old, '20260909-6774-rank21-light-window'), str(source), 'exec'),
    {'__file__':str(source), '__name__':'__main__'})
