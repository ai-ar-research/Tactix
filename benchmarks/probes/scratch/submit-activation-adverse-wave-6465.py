#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-collapse-wave-6333.py')
code=source.read_text(); old='20260909-6332-collapse-three-wave-runtime'; assert code.count(old)==1
code=code.replace(old,'20260909-6464-activation-adverse-three-wave-runtime')
code=code.replace("('pending_validation','checked_UNSAT')","('pending_validation','checked_UNSAT','checked_SAT')")
anchor="m=json.loads((c/'manifest.json').read_text())"; assert code.count(anchor)==1
code=code.replace(anchor,anchor+"\nbinding=m['complete50_selection']; assert sha(root/binding['path'])==binding['sha256']")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
