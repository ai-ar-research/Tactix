#!/usr/bin/env python3
"""Submit only the missing-state paired diagnostic,not a timing refresh."""
from pathlib import Path
source = Path(__file__).with_name('submit-root-lock-rank37-6072.py')
code = source.read_text().replace('20260908-6070-root-lock-rank37-prefix', '20260908-6076-root-lock-rank37-choice')
code = code.replace('Cached native reused.', 'Native reruns only missing heap/choice fields;fixed runtime vector unchanged.')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
