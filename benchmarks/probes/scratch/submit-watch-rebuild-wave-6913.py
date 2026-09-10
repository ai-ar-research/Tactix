#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-vivify-snapshot-wave-6832.py')
code = source.read_text()
for before,after in {
    '20260910-6828-vivify-snapshot-BC-gate':'20260910-6907-watch-rebuild-BC-gate',
    '20260910-6831-vivify-snapshot-runtime':'20260910-6912-watch-rebuild-runtime',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
