#!/usr/bin/env python3
"""Record selected linked kernel identity or displacement without tuning layout."""
from pathlib import Path
source = Path(__file__).with_name('compare-factor-kernel-code-6942.py')
code = source.read_text()
for before,after in {
    '20260910-6925-factor-count-build':'20260910-6950-xor-presence-build',
    'kernel-code-6942.json':'kernel-code-6964.json',
    '6925-factor-count':'6950-xor-presence',
    '_ZN6tactix33kernel_propagate_beyond_conflictsERNS_6KernelEb':'_ZN6tactix33kernel_propagate_beyond_conflictsERNS_6KernelE',
    "assert records['previous']==records['candidate'] and bodies['previous']==bodies['candidate']":
        "identical = records['previous']==records['candidate'] and bodies['previous']==bodies['candidate']",
    'dict(hashes=hashes,records=records,':'dict(hashes=hashes,records=records,selected_bodies_identical=identical,',
    "finding='Five linked propagation/reconnect symbols retain exact addresses, sizes, instruction bytes and printed operands. This differs from6902 kernel-source experiment; no unmeasured alignment rescue or code adjustment was used.'":
        "finding='Selected linked propagation/reconnect addresses, sizes, instruction bytes and operands compared directly. See identity result; no code placement adjustment or alignment rescue.'",
    "print('PASS5 linked propagation/reconnect bodies: exact bytes, addresses and operands')":
        "print('Five linked propagation/reconnect bodies identical:',identical)",
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
