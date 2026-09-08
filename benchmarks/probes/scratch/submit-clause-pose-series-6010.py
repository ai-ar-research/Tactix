#!/usr/bin/env python3
"""Submit the new selected-clause phase-history diagnostic only."""
from pathlib import Path
source = Path(__file__).with_name('submit-forward-caller-5860.py')
code = source.read_text()
begin = code.index('names = {'); end = code.index('\nfor name in names[', begin)
code = code[:begin] + "names = {'series': ['20260908-6008-clause-pose-series']}" + code[end:]
code = code.replace('Independent5849 successor', 'Read-only6008 selected-clause history')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
