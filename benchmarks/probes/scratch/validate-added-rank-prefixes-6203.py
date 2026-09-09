#!/usr/bin/env python3
"""Reuse complete-stream framing checks for the two added-rank prefixes."""
from pathlib import Path
source = Path(__file__).with_name('validate-priority-prefixes-6142.py')
code = source.read_text()
changes = {
    '((6139, 21), (6140, 4))': '((6200, 50), (6201, 49))',
    "f'benchmarks/runs/20260908-{number}-rank{rank}-current-prefix'": "f'benchmarks/runs/20260909-{number}-rank{rank}-current-prefix'",
    '    hashes = {}': "    for rel, digest in m['earlier_closure'].items():\n        assert sha(root / rel) == digest, rel\n    hashes = {}",
}
for old, new in changes.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
