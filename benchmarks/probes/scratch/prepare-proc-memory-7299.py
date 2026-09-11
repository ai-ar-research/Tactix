#!/usr/bin/env python3
"""Preserve sandbox-denied7298 preflight and prepare a separate allowed run."""
from pathlib import Path
base = Path(__file__).with_name('prepare-proc-memory-7298.py')
code = base.read_text().replace('7298', '7299')
needle = "m.update(schema='tactix-external-proc-memory-v1',"
assert code.count(needle) == 1
code = code.replace(needle, """failed = root / 'benchmarks/runs/20260911-7298-proc-memory-rank31'
for p in (failed / 'logs').iterdir():
    bind(p)
bind(Path(__file__).with_name('prepare-proc-memory-7298.py'))
m['prior_preflight_failure'] = '7298 fixed Python fixture could not start: sandbox denied PTRACE_TRACEME. No SAT solver ran, no job submitted. Original files preserved.7299 repeats only the fixed preflight outside the ptrace sandbox before submission.'
""" + needle)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
