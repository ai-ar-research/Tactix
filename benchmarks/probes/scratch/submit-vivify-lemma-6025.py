#!/usr/bin/env python3
"""Submit the separate native-only learned-clause history."""
from pathlib import Path
source = Path(__file__).with_name('submit-forward-caller-5860.py')
code = source.read_text()
begin = code.index('names = {'); end = code.index('\nfor name in names[', begin)
code = code[:begin] + "names = {'lemma': ['20260908-6024-vivify-lemma-history']}" + code[end:]
code = code.replace('Independent5849 successor', 'Read-only6024 learned-clause history with frozen6016 pins')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
