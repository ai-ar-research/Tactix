#!/usr/bin/env python3
"""Correct the collector witness input order; retain failed 7344 unchanged."""
from pathlib import Path
base = Path(__file__).with_name('check-debug-contract-proofs-7344.py')
code = base.read_text()
for before, after in {
    '20260911-7344-debug-contract-proofs': '20260911-7345-debug-contract-proofs',
    '[[sign],[sign,4,5],[4,5,6],[-4,7]]': '[[sign,4,5],[4,5,6],[-4,7],[sign]]',
    'for path in [Path(__file__),driver,': 'for path in [Path(__file__),Path(__file__).with_name("check-debug-contract-proofs-7344.py"),driver,',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
