#!/usr/bin/env python3
"""Finish the already-staged audited checkpoint; retain the whitespace gate."""
from pathlib import Path
source = Path(__file__).with_name('land-watermark-followup-6762.py')
code = source.read_text()
for before,after in {
    '2026-09-09-watermark-followup-audit-6761.json':'2026-09-10-factor-xor-landing-audit-6973.json',
    'Audit full50 watermark costs and localize Rank21 search split':
        'Measure Factor counter cost and reuse indexed XOR parity membership',
    "assert not subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root)":
        "assert set(subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).splitlines()) <= set(owned), 'preserve unrelated staged work'",
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
