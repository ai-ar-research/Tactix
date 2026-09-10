#!/usr/bin/env python3
"""Strict unchanged-work gate, read-only unless explicitly sealed once."""
from pathlib import Path
source = Path(__file__).with_name('validate-bve-views-BC-7104.py')
code = source.read_text().replace('20260910-7103-bve-views-BC-gate', '20260910-7122-bve-import-BC-gate')
code = code.replace('neutrality-7104.json', 'neutrality-7123.json')
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
