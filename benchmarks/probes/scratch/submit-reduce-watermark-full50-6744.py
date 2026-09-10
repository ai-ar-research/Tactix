#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-congruence-empty-full50-6667.py')
code=source.read_text();old='20260909-6666-congruence-empty-ranks1-50';assert code.count(old)==1
exec(compile(code.replace(old,'20260909-6743-reduce-watermark-ranks1-50'),str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
