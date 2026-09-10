#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-vivify-snapshot-BC-6829.py')
code = source.read_text()
before = '20260910-6828-vivify-snapshot-BC-gate'
assert before in code
code = code.replace(before,'20260910-6933-factor-count-BC-gate')
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
