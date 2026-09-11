#!/usr/bin/env python3
"""Exact full50 comparison, accepting pending parent checks only as evidence."""
from pathlib import Path
base = Path(__file__).with_name('validate-vivify-revalidation-full50-7168.py')
code = base.read_text()
changes = {
    '20260910-7167-vivify-revalidation-ranks1-50':'20260911-7201-factor-arena-ranks1-50',
    '20260910-7053-collect-retry-ranks1-50':'20260910-7167-vivify-revalidation-ranks1-50',
    '2026-09-10-vivify-revalidation-full50-7168':'2026-09-11-factor-arena-full50-7201',
    "    old = json.loads(p.read_text())\n    assert classify(old, True)['checked'] and classify(old, True)['clean']":
        "    parent_final = p.exists()\n    if not parent_final: p = parent / f'provisional/{task:03d}.json'\n    old = json.loads(p.read_text())\n    assert classify(old, parent_final)['status'] in ('checked_SAT','checked_UNSAT','pending_validation')",
    'One fresh full-set transfer view against fixed native and checked7030;':'One fresh arena transfer against fixed native and retained7144; parent proofs may remain pending;',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
