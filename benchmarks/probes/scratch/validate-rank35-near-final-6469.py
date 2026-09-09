#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('validate-rank35-later-6461.py')
code=source.read_text()
for old,new in {
 '20260909-6459-rank35-later-prefix':'20260909-6467-rank35-near-final-prefix',
 '20260909-6389-rank35-stamp-extension':'20260909-6459-rank35-later-prefix',
 '99197378':'159573175',
 "'140843d3c85c31da'":"'4bfd01ec4333149f'",
 '"\'^1000000\\\\t\'"':'"\'^3000000\\\\t\'"',
 '"b\'1000000\\\\t\'"':'"b\'3000000\\\\t\'"',
 "'stop=3000000'":"'stop=4400000'",
 'old1M decision closure':'old3M decision closure',
}.items():
    assert old in code,old; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
