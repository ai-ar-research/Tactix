#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('land-retry-full50-6260.py')
code = source.read_text()
code = code.replace('2026-09-09-retry-full50-audit-6259.json', '2026-09-09-vivify-binary-audit-6281.json')
code = code.replace('Record full Rank 1-50 repair outcome and remaining retry omissions', 'Restore retained-endpoint retry advice for binary Vivify')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
