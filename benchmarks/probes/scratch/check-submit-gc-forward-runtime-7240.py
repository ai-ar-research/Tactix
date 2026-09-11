#!/usr/bin/env python3
from pathlib import Path
base = Path(__file__).with_name('check-submit-factor-arena-runtime-7186.py')
code = base.read_text()
for before,after in {
    '20260910-7186-factor-arena-runtime':'20260911-7240-gc-forward-runtime',
    'check-submit-factor-arena-BC-7183.py':'check-submit-gc-forward-BC-7238.py',
    'Factor runtime':'GC forward runtime',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
