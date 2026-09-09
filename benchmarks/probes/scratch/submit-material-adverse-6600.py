#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-collapse-wave-6333.py')
code=source.read_text()
for old,new in {
 '20260909-6332-collapse-three-wave-runtime':'20260909-6599-material-adverse-rank38',
 'range(6*wave)':'range(2*wave)',
 '6*wave+5':'2*wave+1',
 '6*wave+6':'2*wave+2',
 '6*wave':'2*wave',
}.items():
    assert old in code,old
    code=code.replace(old,new)
anchor="m=json.loads((c/'manifest.json').read_text())"; assert code.count(anchor)==1
code=code.replace(anchor,anchor+"\nb=m['complete50_selection']; assert sha(root/b['path'])==b['sha256']")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
