#!/usr/bin/env python3
"""Submit only the newly prepared, independently pinned composition gates."""
from pathlib import Path
source = Path(__file__).with_name('submit-reduce-root-5908.py')
code = source.read_text()
for old, new in {
    '20260908-5906-reduce-root-BC-gate':'20260908-5924-reduce-preclean-BC-gate',
    '20260908-5907-reduce-root-rank32-prefix':'20260908-5925-reduce-preclean-rank32-prefix',
    '20260908-5908-reduce-root-ranks1-40':'20260908-5926-reduce-preclean-ranks1-40',
    'Independent5900 successor on5849;regressing5870 remains separate':
        'Isolated5920 composition on5900;all old pins and campaigns remain unchanged',
}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
