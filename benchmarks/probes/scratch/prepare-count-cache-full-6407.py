#!/usr/bin/env python3
"""All50 stay scored against the same matched reference after the B/C guard."""
from pathlib import Path
source=Path(__file__).with_name('prepare-compact-stamp-full-6366.py')
root=source.resolve().parents[3]
gate=root/'benchmarks/runs/20260909-6387-vivify-count-cache-BC-gate/solver-checkpoint-6392.json'
if not gate.exists():
    print('Complete exact-work B/C checkpoint pending; no campaign created')
    raise SystemExit(0)
code=source.read_text()
for old,new in {
    '20260909-6362-compact-stamp-BC-gate/solver-checkpoint-6364.json':'20260909-6387-vivify-count-cache-BC-gate/solver-checkpoint-6392.json',
    "assert g['solver_records'] == 66":"assert g['solver_records'] == 66 and g['exact_candidate_parent_work_and_proof']",
    '20260909-6366-compact-stamp-ranks1-50':'20260909-6407-count-cache-ranks1-50',
    '20260909-6357-compact-stamp-build':'20260909-6383-vivify-count-cache-build',
    'build/pin-6357-compact-stamp-':'build/pin-6383-vivify-count-cache-',
    'Faithful detached queue-state preservation repairs a measured source-order key mismatch. Price all50 without assuming a runtime benefit or identical work.':'The private leading-count cache should preserve all6357 proof bytes and non-timing work while reducing some Vivify sorting cost. Score all50 without assuming a gain; independently test complete parent work/proof identity.',
}.items():
    assert old in code,old
    code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
