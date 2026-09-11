#!/usr/bin/env python3
"""Check protected reasons and root/elevated compact/tail proofs on both pins."""
from pathlib import Path
base = Path(__file__).with_name('check-collect-retry-proofs-7033.py')
code = base.read_text()
for before,after in {
    '20260910-7033-collect-retry-proofs':'20260911-7237-gc-forward-root-lock',
    "('current', '6730-reduce-watermark', '20260909')":"('current', '7162-vivify-revalidation', '20260910')",
    "('candidate', '7030-collect-retry', '20260910')":"('candidate', '7233-gc-forward', '20260911')",
    'dict(checked=16':'dict(status="PASS", checked=16',
    'New40-state retry matrix separately pins advice-only effect.':
        'New26240-state watch equivalence and896-state actual parent collector comparisons are owned by7233; this gate verifies certificates through real solve completion.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
