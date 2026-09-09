#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-material-rank31-6606.py')
code = source.read_text()
old = '20260909-6605-material-rank31-prefix'
assert code.count(old) == 1
code = code.replace(old, '20260909-6637-rank35-binary-life')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
