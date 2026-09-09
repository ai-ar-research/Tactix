#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-substitute-retry-gates-6238.py')
code = source.read_text()
for a,b in (('20260909-6236-substitute-retry-BC-gate', '20260909-6275-vivify-binary-BC-gate'),
            ('20260909-6237-substitute-retry-rank32-prefix', '20260909-6276-vivify-binary-rank32-prefix')):
    assert code.count(a) == 1; code = code.replace(a,b)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
