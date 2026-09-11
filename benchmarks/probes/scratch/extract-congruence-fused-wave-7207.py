#!/usr/bin/env python3
from pathlib import Path
base = Path(__file__).with_name('extract-vivify-compact-wave-7153.py')
code = base.read_text()
for before,after in {
    'validate-vivify-compact-profiles-7152.py':'validate-congruence-fused-profiles-7206.py',
    '20260910-7153-vivify-compact-samples':'20260911-7207-congruence-fused-samples',
    "'tactix::vivify_apply('":"'tactix::congruence_apply('",
    "components.get('vivify', 0)":"components.get('congruence', 0)",
    'complete Vivify cycles':'complete Congruence cycles',
    'complete_vivify_ratio=':'complete_congruence_ratio=',
}.items():
    assert code.count(before) == 1, before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
