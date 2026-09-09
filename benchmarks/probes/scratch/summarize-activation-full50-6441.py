#!/usr/bin/env python3
"""Complete per-rank runtime, work and memory view; changed work stays visible."""
from pathlib import Path
source=Path(__file__).with_name('summarize-compact-stamp-full50-6371.py')
code=source.read_text()
for old,new in {
 '20260909-6366-compact-stamp-ranks1-50':'20260909-6439-activation-ranks1-50',
 '20260909-6320-substitute-collapse-ranks1-50':'20260909-6407-count-cache-ranks1-50',
 '2026-09-09-compact-stamp-full50-6371':'2026-09-09-activation-full50-6441',
}.items():
    assert old in code; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
