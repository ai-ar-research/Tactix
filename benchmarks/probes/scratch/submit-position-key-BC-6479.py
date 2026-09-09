#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-shared-blocker-BC-6452.py')
code=source.read_text(); old='20260909-6451-shared-blocker-BC-gate'; assert code.count(old)==1
code=code.replace(old,'20260909-6478-position-key-BC-gate')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
