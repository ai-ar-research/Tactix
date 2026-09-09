#!/usr/bin/env python3
"""Submit only the newly frozen repair gates and diagnostic."""
from pathlib import Path
source=Path(__file__).with_name('submit-forward-caller-5860.py')
code=source.read_text();begin=code.index('names = {');end=code.index('\nfor name in names[',begin)
code=code[:begin]+"names = {'gate': ['20260908-6059-root-lock-BC-gate','20260908-6060-root-lock-rank32-prefix'], 'full': ['20260908-6061-root-lock-ranks1-40']}"+code[end:]
code=code.replace('Independent5849 successor','Independent6049 root-lock repair')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
