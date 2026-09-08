#!/usr/bin/env python3
"""Bind the second exact input-empty-clause status incident; no runner edits."""
from pathlib import Path
source=Path(__file__).with_name('classify-trivial-checker-5874.py')
code=source.read_text().replace('2026-09-08-trivial-checker-status.json','2026-09-08-trivial-checker-status-second.json')
code=code.replace('829','1281')
old='[p, cnf, checker, source, *fixtures.iterdir()]'
assert old in code
code=code.replace(old,"[p, c / 'results/1280.json', cnf, checker, source, *fixtures.iterdir()]")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
