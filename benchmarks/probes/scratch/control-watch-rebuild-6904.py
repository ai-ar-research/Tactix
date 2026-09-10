#!/usr/bin/env python3
"""Correct fixture API spelling; retain6903 compile-only failure evidence."""
from pathlib import Path
source = Path(__file__).with_name('control-watch-rebuild-6903.py')
code = source.read_text()
assert '6903' in code
code = code.replace('6903','6904')
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
