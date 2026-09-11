#!/usr/bin/env python3
from pathlib import Path
base = Path(__file__).with_name('check-submit-factor-arena-BC-7183.py')
code = base.read_text().replace('20260910-7183-factor-arena-BC','20260911-7202-congruence-fused-BC').replace('Factor arena BC:', 'Congruence fused BC:')
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
