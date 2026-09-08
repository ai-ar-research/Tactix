#!/usr/bin/env python3
"""Submit only the independent reinsertion-order diagnostic."""
from pathlib import Path
source = Path(__file__).with_name('submit-forward-caller-5860.py')
code = source.read_text()
begin = code.index('names = {')
end = code.index('\nfor name in names[', begin)
code = code[:begin] + "names = {'order': ['20260908-5996-bve-reinsert-order']}" + code[end:]
code = code.replace('Independent5849 successor', 'Read-only5996 reinsertion diagnostic')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
