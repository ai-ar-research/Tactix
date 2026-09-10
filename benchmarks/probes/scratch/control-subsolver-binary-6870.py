#!/usr/bin/env python3
"""Actual old/new full-width binary-tagged watch controls."""
from pathlib import Path
source = Path(__file__).with_name('control-subsolver-shrink-6858.py')
code = source.read_text()
for before,after in {
    '20260910-6857-subsolver-shrink-build':'20260910-6869-subsolver-binary-build',
    'control-6858.json':'control-6870.json',
    'runtime-subsolver-shrink-6857':'runtime-subsolver-binary-6869',
    'test_subsolver_shrink':'test_subsolver_binary',
    'fixed incremental watch-compaction sequences':'fixed incremental binary-watch sequences at reference cache-line boundaries',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
