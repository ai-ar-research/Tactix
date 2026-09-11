#!/usr/bin/env python3
"""Read-only exact-work B/C validation; optional one-time solver checkpoint."""
from pathlib import Path
template = Path(__file__).with_name('validate-bve-views-BC-7104.py')
code = template.read_text()
code = code.replace('20260910-7103-bve-views-BC-gate', '20260910-7166-vivify-revalidation-BC-gate')
code = code.replace('neutrality-7104.json', 'neutrality-7166.json')
exec(compile(code, str(template), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
