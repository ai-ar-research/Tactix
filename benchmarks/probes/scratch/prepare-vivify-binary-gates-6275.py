#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('prepare-substitute-retry-gates-6236.py')
code = source.read_text()
changes = {
    '20260909-6231-substitute-retry-build': '20260909-6269-vivify-binary-retry-build',
    '20260909-6234-substitute-retry-references': '20260909-6274-vivify-binary-references',
    '20260909-6236-substitute-retry-BC-gate': '20260909-6275-vivify-binary-BC-gate',
    '20260909-6237-substitute-retry-rank32-prefix': '20260909-6276-vivify-binary-rank32-prefix',
    'build/pin-6231-substitute-retry-': 'build/pin-6269-vivify-binary-retry-',
    'tactix-6237-': 'tactix-6276-', '6237-rank32': '6276-rank32',
    "'337674'": "'399915'",
    'canonical Substitute removal publication': 'binary-Vivify removal publication',
    'canonical Substitute retry repair': 'binary-Vivify retry repair',
    'new6231 source': 'new6269 source',
}
for a,b in changes.items(): assert a in code, a; code = code.replace(a,b)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
