#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-congruence-time-6536.py')
code = source.read_text()
assert code.count('20260909-6535-rank32-congruence-time') == 1
code = code.replace('20260909-6535-rank32-congruence-time', '20260909-6539-rank32-early-congruence-time')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
