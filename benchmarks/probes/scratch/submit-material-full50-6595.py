#!/usr/bin/env python3
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
subprocess.run(['python3',str(Path(__file__).with_name('validate-material-schedule-BC-6586.py'))],check=True)
source=Path(__file__).with_name('submit-compact-stamp-followup-6367.py')
code=source.read_text().replace('20260909-6366-compact-stamp-ranks1-50','20260909-6594-material-ranks1-50')
anchor="gate = m['provisional_gate']"; assert code.count(anchor)==1
code=code.replace(anchor,"assert sys.argv[1]=='full'\nfor binding in m['causal_closures'].values(): assert sha(root/binding['path'])==binding['sha256']\nbinding=m['paired_timing']; assert sha(root/binding['path'])==binding['sha256']\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
