#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-rank45-stamp-prefix-6403.py')
code=source.read_text(); old='20260909-6402-rank45-stamp-prefix'; assert code.count(old)==1
code=code.replace(old,'20260909-6470-rank33-activation-prefix')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
