#!/usr/bin/env python3
"""Correct6942's no-argument symbol spelling; no binary or output was changed."""
from pathlib import Path
source = Path(__file__).with_name('compare-factor-kernel-code-6942.py')
code = source.read_text()
before = '_ZN6tactix33kernel_propagate_beyond_conflictsERNS_6KernelEb'
assert before in code
code = code.replace(before,'_ZN6tactix33kernel_propagate_beyond_conflictsERNS_6KernelE')
code = code.replace('kernel-code-6942.json','kernel-code-6943.json')
code = code.replace('dict(hashes=hashes,records=records,',
    "dict(hashes=hashes,records=records,preflight_classification='6942 expected a nonexistent boolean argument in the beyond-conflicts mangled name; failed symbol-set assertion before objdump, artifact creation, solver execution or mutation. Correct no-argument symbol obtained from actual nm; all failed targets remain unwritten.',")
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
