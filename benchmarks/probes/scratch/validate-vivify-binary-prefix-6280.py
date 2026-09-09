#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('validate-substitute-retry-prefix-6246.py')
code = source.read_text()
old = '20260909-6237-substitute-retry-rank32-prefix'
assert code.count(old) == 1
exec(compile(code.replace(old, '20260909-6276-vivify-binary-rank32-prefix'), str(source), 'exec'),
     {'__file__': str(source), '__name__': '__main__'})
