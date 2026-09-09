#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-rank32-transitive-6341.py')
code=source.read_text();old='20260909-6340-rank32-transitive-census'
assert code.count(old)==1
code=code.replace(old,'20260909-6352-rank32-transitive-schedule')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
