#!/usr/bin/env python3
"""Submit only the independent learning-stage diagnostic."""
from pathlib import Path
source = Path(__file__).with_name('submit-forward-caller-5860.py')
code = source.read_text()
begin = code.index('names = {'); end = code.index('\nfor name in names[', begin)
code = code[:begin] + "names = {'learned': ['20260908-6035-learned-stage']}" + code[end:]
code = code.replace('Independent5849 successor', 'Read-only6035 learning stages')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
