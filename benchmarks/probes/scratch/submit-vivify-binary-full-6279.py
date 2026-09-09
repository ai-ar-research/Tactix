#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-learned-sort-full-6181.py')
code = source.read_text(); old = '20260909-6181-learned-sort-ranks1-50'
assert code.count(old) == 1
exec(compile(code.replace(old, '20260909-6278-vivify-binary-ranks1-50'), str(source), 'exec'),
     {'__file__': str(source), '__name__': '__main__'})
