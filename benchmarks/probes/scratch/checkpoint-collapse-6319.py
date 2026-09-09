#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('checkpoint-collapse-6314.py')
code=source.read_text();old='2026-09-09-collapse-checkpoint-6314.json'
assert code.count(old)==1
exec(compile(code.replace(old,'2026-09-09-collapse-checkpoint-6319.json'),str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
