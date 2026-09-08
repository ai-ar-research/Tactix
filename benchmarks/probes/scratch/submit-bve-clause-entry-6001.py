#!/usr/bin/env python3
"""Submit only the independently pinned first-forward entry snapshot."""
from pathlib import Path
source = Path(__file__).with_name('submit-forward-caller-5860.py')
code = source.read_text()
begin = code.index('names = {')
end = code.index('\nfor name in names[', begin)
code = code[:begin] + "names = {'entry': ['20260908-6000-bve-clause-entry']}" + code[end:]
code = code.replace('Independent5849 successor', 'Read-only6000 clause-entry diagnostic')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
