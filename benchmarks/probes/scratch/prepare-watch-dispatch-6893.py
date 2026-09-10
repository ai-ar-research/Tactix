#!/usr/bin/env python3
"""Independent same-layout mixed-watch dispatch experiment on6730."""
from pathlib import Path
source = Path(__file__).with_name('prepare-subsolver-shrink-6857.py')
code = source.read_text()
for before,after in {
    'runtime-subsolver-shrink-6857':'runtime-watch-dispatch-6893',
    '20260910-6857-subsolver-shrink-build':'20260910-6893-watch-dispatch-build',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
