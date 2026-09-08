#!/usr/bin/env python3
"""Submit only new5962 campaigns; prefix/full follow expected BC solver gate."""
from pathlib import Path

source = Path(__file__).with_name('submit-forward-caller-5860.py')
code = source.read_text()
begin = code.index('names = {')
end = code.index('\nfor name in names[', begin)
code = code[:begin] + """names = {
    'gate': ['20260908-5968-bve-active-pop-BC-gate'],
    'prefix': ['20260908-5969-bve-active-pop-rank32-prefix'],
    'ancestry': ['20260908-5972-bve-sift-ancestry'],
    'earlier': ['20260908-5978-bve-earlier-heap'],
    'full': ['20260908-5970-bve-active-pop-ranks1-40']}""" + code[end:]
code = code.replace('Independent5849 successor', 'Independent5962 successor of5900')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
