#!/usr/bin/env python3
"""Submit only the predeclared two-task diagnostic pair."""
from pathlib import Path
base=Path(__file__).with_name('submit-vivify-compact-profile-wave-7152.py')
code=base.read_text()
code=code.replace('validate-vivify-compact-profiles-7152.py','validate-realloc-rank38-profiles-7278.py')
before="sys.argv[1] in ('0', '1', '2')"
assert code.count(before)==1
code=code.replace(before,"sys.argv[1] == '0'")
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
