#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('prepare-substitute-retry-gates-6236.py')
code=source.read_text()
marker="\nold = root/'benchmarks/runs/20260909-6183-rank32-current-prefix'"
assert code.count(marker)==1;code=code.split(marker)[0]
for a,b in (('20260909-6231-substitute-retry-build','20260909-6305-substitute-collapse-build'),
            ('20260909-6234-substitute-retry-references','20260909-6309-collapse-reference-fixtures'),
            ('20260909-6236-substitute-retry-BC-gate','20260909-6311-substitute-collapse-BC-gate'),
            ('build/pin-6231-substitute-retry-','build/pin-6305-substitute-collapse-'),
            ('canonical Substitute removal publication','canonical large-to-binary retained-endpoint retry advice')):
    assert a in code,a;code=code.replace(a,b)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
