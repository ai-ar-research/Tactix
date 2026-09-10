#!/usr/bin/env python3
"""Freeze all50 results, including regressions, against6654 and fixed Kissat."""
from pathlib import Path
source=Path(__file__).with_name('summarize-compact-stamp-full50-6371.py')
code=source.read_text()
for before,after in {
 '20260909-6366-compact-stamp-ranks1-50':'20260909-6743-reduce-watermark-ranks1-50',
 '20260909-6320-substitute-collapse-ranks1-50':'20260909-6666-congruence-empty-ranks1-50',
 '2026-09-09-compact-stamp-full50-6371':'2026-09-09-reduce-watermark-full50-6750'}.items():
    assert code.count(before)==1;code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
