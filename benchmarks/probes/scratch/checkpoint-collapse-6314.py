#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('checkpoint-substitute-retry-6247.py')
code=source.read_text()
for a,b in (('2026-09-09-substitute-retry-checkpoint-6247.json','2026-09-09-collapse-checkpoint-6314.json'),
            ('20260909-6236-substitute-retry-BC-gate','20260909-6311-substitute-collapse-BC-gate'),
            ('20260909-6239-substitute-retry-ranks1-50','20260909-6278-vivify-binary-ranks1-50')):
    assert a in code;code=code.replace(a,b)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
