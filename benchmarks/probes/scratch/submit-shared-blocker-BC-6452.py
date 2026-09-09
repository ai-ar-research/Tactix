#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-compact-stamp-BC-6363.py')
code=source.read_text(); old='20260909-6362-compact-stamp-BC-gate'; assert code.count(old)==1
code=code.replace(old,'20260909-6451-shared-blocker-BC-gate')
anchor="authority = root/m['candidate_authority']/'manifest.json'"
assert code.count(anchor)==1
code=code.replace(anchor,"assert sha(root/m['actual_parent_control']['path'])==m['actual_parent_control']['sha256']\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
