#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('summarize-vivify-snapshot-waves-6833.py')
code = source.read_text()
for before,after in {
    '20260910-6831-vivify-snapshot-runtime':'20260910-6912-watch-rebuild-runtime',
    'solver-checkpoint-6833.json':'solver-checkpoint-6914.json',
    'Same-work snapshot omission6822':'Same-work binary-only reconnect scan omission6902',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
