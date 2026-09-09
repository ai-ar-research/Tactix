#!/usr/bin/env python3
"""Reuse the guarded exact-pin launch protocol for the selected-pair successor."""
from pathlib import Path
source=Path(__file__).with_name('submit-rank32-transitive-6341.py')
code=source.read_text()
old='20260909-6340-rank32-transitive-census'
assert code.count(old)==1
code=code.replace(old,'20260909-6345-rank32-selected-transitive')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
