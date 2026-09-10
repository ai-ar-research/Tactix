#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('validate-rank21-light-window-6776.py')
code = source.read_text()
for before, after in {
    '20260909-6774-rank21-light-window': '20260909-6782-rank21-trajectory-stop',
    'summary-6776.json': 'summary-6784.json',
    'finite event-stop one past50K': 'unreachable finite event-stop UINT64_MAX-1'}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
