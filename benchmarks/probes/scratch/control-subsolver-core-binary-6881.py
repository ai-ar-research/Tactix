#!/usr/bin/env python3
"""Correct the nested fixture-main macro; preserve failed pre-solver6879."""
from pathlib import Path
source = Path(__file__).with_name('control-subsolver-core-binary-6879.py')
code = source.read_text()
for before,after in {
    'control-6879.json':'control-6881.json',
    'core-own-arm-':'core-fixed-own-arm-',
    'test-subsolver-core-binary-6878.cpp':'test-subsolver-core-binary-6880.cpp',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
