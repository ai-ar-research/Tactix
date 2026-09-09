#!/usr/bin/env python3
"""Count physical clauses after frontend duplicate normalization, not CNF lines."""
from pathlib import Path
source=Path(__file__).with_name('check-substitute-retry-proofs-6241.py')
code=source.read_text()
for old,new in (('20260909-6241-substitute-retry-fixed-proofs','20260909-6244-substitute-retry-fixed-proofs'),
                ('test-substitute-retry-proof-model-6241.cpp','test-substitute-retry-proof-model-6244.cpp')):
    assert code.count(old)==1
    code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
