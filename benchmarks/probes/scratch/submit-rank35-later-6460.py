#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-rank35-stamp-prefix-6377.py')
code=source.read_text(); old='20260909-6376-rank35-stamp-prefix'; assert code.count(old)==1
code=code.replace(old,'20260909-6459-rank35-later-prefix')
anchor='since = dt.datetime.now()'; assert code.count(anchor)==1
code=code.replace(anchor,"binding=m['provisional_gate']; assert sha(root/binding['path'])==binding['sha256']\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
