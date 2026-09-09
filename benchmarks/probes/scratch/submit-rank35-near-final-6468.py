#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-rank35-later-6460.py')
code=source.read_text(); old='20260909-6459-rank35-later-prefix'; assert code.count(old)==1
code=code.replace(old,'20260909-6467-rank35-near-final-prefix')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
