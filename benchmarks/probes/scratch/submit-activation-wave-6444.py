#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-collapse-wave-6333.py')
code=source.read_text(); old='20260909-6332-collapse-three-wave-runtime'; assert code.count(old)==1
code=code.replace(old,'20260909-6443-activation-three-wave-runtime')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
