#!/usr/bin/env python3
"""Land only the audited isolated cost checkpoint; no denied push retry."""
from pathlib import Path
source = Path(__file__).with_name('land-watermark-followup-6762.py')
code = source.read_text()
for before,after in {
    '2026-09-09-watermark-followup-audit-6761.json': '2026-09-10-sweep-rootfree-audit-6854.json',
    'Audit full50 watermark costs and localize Rank21 search split': 'Test Sweep copy cost and retain scheduling/CDCL parity gates',
}.items():
    assert before in code
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
