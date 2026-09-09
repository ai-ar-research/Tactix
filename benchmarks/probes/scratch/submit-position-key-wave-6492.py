#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-activation-adverse-wave-6465.py')
code=source.read_text()
old='20260909-6464-activation-adverse-three-wave-runtime'; assert code.count(old)==1
code=code.replace(old,'20260909-6491-position-key-three-wave-runtime')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
