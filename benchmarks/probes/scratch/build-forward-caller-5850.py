#!/usr/bin/env python3
"""Build and validate three independent candidate images."""
from pathlib import Path

here = Path(__file__).resolve().parent
source = here / 'build-forward-admission-5831.py'
code = source.read_text().replace('forward-admission', 'forward-caller')
for old, new in [('5830', '5849'), ('5831', '5850'), ('5832', '5851')]:
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
