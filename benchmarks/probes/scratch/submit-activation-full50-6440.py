#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-compact-stamp-followup-6367.py')
code=source.read_text()
old='20260909-6366-compact-stamp-ranks1-50'; assert code.count(old)==1
code=code.replace(old,'20260909-6439-activation-ranks1-50')
anchor="gate = m['provisional_gate']"; assert code.count(anchor)==1
code=code.replace(anchor,"assert sys.argv[1]=='full'\nfor binding in m['causal_closures'].values(): assert sha(root/binding['path'])==binding['sha256']\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
