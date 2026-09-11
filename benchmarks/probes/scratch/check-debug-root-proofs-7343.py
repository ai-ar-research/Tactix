#!/usr/bin/env python3
"""Repeat root-fold proof/model gates on the collector-contract successor."""
from pathlib import Path
base = Path(__file__).with_name('check-root-watermark-proofs-7336.py')
code = base.read_text()
for before, after in {
    '20260911-7336-root-watermark-proofs': '20260911-7343-debug-root-proofs',
    '20260911-7335-root-watermark-build': '20260911-7340-debug-contracts-build',
    'build/pin-7335-root-watermark-': 'build/pin-7340-debug-contracts-',
    'for path in [Path(__file__),': 'for path in [Path(__file__), Path(__file__).with_name("check-root-watermark-proofs-7336.py"),',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
