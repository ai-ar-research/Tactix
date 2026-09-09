#!/usr/bin/env python3
"""Complete the third pure-retention site's offline sample attribution."""
from pathlib import Path

source = Path(__file__).with_name('annotate-retention-6530.py')
code = source.read_text()
start = code.index('symbols = {\n')
end = code.index('\noutputs = {}', start)
code = code[:start] + "symbols = {'candidate': {'drop': 'tactix::kernel_drop_large_watches(tactix::Kernel&)'}}" + code[end:]
for old, new in {
    '20260909-6530-rank32-retention-annotation': '20260909-6532-rank32-drop-annotation',
    "enumerate(('candidate', 'kissat'))": "enumerate(('candidate',))",
    'PASS: eight annotations, exact inputs, owned temporary expansions deleted':
        'PASS: two drop annotations, exact inputs, owned temporary expansion deleted',
}.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
