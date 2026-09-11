#!/usr/bin/env python3
from pathlib import Path
base = Path(__file__).with_name('validate-root-binary-profiles-7230.py')
code = base.read_text().replace('7230-root-binary', '7241-gc-forward').replace('solver-summary-7230.json', 'solver-summary-7241.json').replace('Unprofiled7226', 'Unprofiled7240')
exec(compile(code,str(base),'exec'), globals())
