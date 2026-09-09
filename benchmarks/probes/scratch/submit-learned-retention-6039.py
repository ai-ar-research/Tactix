#!/usr/bin/env python3
"""Submit the isolated post-learning Reduce trace."""
from pathlib import Path
source = Path(__file__).with_name('submit-forward-caller-5860.py')
code = source.read_text()
begin = code.index('names = {'); end = code.index('\nfor name in names[', begin)
code = code[:begin] + "names = {'retention': ['20260908-6038-learned-retention']}" + code[end:]
code = code.replace('Independent5849 successor', 'Read-only6038 learned-clause retention')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
