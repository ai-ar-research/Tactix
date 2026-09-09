#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('checkpoint-vivify-binary-6284.py')
code = source.read_text()
assert code.count('2026-09-09-vivify-binary-checkpoint-6284.json') == 1
exec(compile(code.replace('2026-09-09-vivify-binary-checkpoint-6284.json', '2026-09-09-vivify-binary-checkpoint-6294.json'), str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
