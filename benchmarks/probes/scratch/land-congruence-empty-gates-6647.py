#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('land-retry-full50-6260.py')
code = source.read_text()
for before,after in {
    '2026-09-09-retry-full50-audit-6259.json':'2026-09-09-congruence-empty-gates-audit-6646.json',
    'Record full Rank 1-50 repair outcome and remaining retry omissions':'Repair gate-free Congruence cleanup and validate Rank49 convergence witness',
}.items():
    assert code.count(before) == 1
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
