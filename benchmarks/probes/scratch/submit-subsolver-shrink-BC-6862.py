#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-sweep-rootfree-BC-6847.py')
code = source.read_text()
before = '20260910-6846-sweep-rootfree-BC-gate'
assert before in code
code = code.replace(before,'20260910-6861-subsolver-shrink-BC-gate')
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
