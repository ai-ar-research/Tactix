#!/usr/bin/env python3
"""Exact all-counter transfer; classify parent and candidate pending checks."""
from pathlib import Path
base=Path(__file__).with_name('validate-vivify-revalidation-full50-7168.py')
code=base.read_text()
for before,after in {
    '20260910-7167-vivify-revalidation-ranks1-50':'20260911-7283-realloc-ranks1-50',
    '20260910-7053-collect-retry-ranks1-50':'20260911-7262-occurrence-ranks1-50',
    '2026-09-10-vivify-revalidation-full50-7168':'2026-09-11-realloc-full50-7283',
    'submit-vivify-compact-wave-7150.py':'occurrence-fingerprint-7256.py',
    'h.fingerprint(':'h.raw_fingerprint(',
    "    old = json.loads(p.read_text())\n    assert classify(old, True)['checked'] and classify(old, True)['clean']":
        "    final=p.exists()\n    if not final: p=parent/f'provisional/{task:03d}.json'\n    old=json.loads(p.read_text()); parent_state=classify(old,final)\n    assert parent_state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,parent_state)",
    "validation=states[task]['status'], exact_parent_work_and_proof=exact,":"validation=states[task]['status'], parent_validation=parent_state['status'], exact_parent_work_and_proof=exact,",
    'One fresh full-set transfer view against fixed native and checked7030;':
        'One full-set allocator transfer against fixed native and own7253; all non-timing counters including physical dense counts exact. Parent and candidate validation states explicit; pending proofs do not prevent provisional work or count as accepted correctness;',
}.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
