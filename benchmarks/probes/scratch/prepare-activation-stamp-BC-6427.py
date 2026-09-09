#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('prepare-substitute-retry-gates-6236.py')
code=source.read_text(); marker="\nold = root/'benchmarks/runs/20260909-6183-rank32-current-prefix'"
assert code.count(marker)==1; code=code.split(marker)[0]
for old,new in {
 '20260909-6231-substitute-retry-build':'20260909-6424-activation-stamp-build',
 '20260909-6234-substitute-retry-references':'20260909-6426-activation-stamp-controls',
 '20260909-6236-substitute-retry-BC-gate':'20260909-6427-activation-stamp-BC-gate',
 'build/pin-6231-substitute-retry-':'build/pin-6424-activation-stamp-',
 'canonical Substitute removal publication':'native first-activation numeric queue stamps',
 'Reference-backed retry repair may change search work. No exact parent work or speedup assumed.':'Source-backed activation numbering can change search work. Keep fullB28+C5 unchanged and compare production; no exact parent trajectory or runtime benefit assumed. Preserve compact stamp retention and public Decide phases.',
}.items():
    assert old in code,old; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
