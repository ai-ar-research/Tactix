#!/usr/bin/env python3
"""Additional direct core-rebuild/shuffle coverage without mutating live pins."""
from pathlib import Path
source = Path(__file__).with_name('control-subsolver-binary-6874.py')
code = source.read_text()
for before,after in {
    "target = c/'control-6874.json'":"target = c/'control-6879.json'",
    "'own-arm-'":"'core-own-arm-'",
    "fixture = root/m['source']/'tests/test_subsolver_binary.cpp'":"fixture = root/'benchmarks/probes/scratch/test-subsolver-core-binary-6878.cpp'\n    flags += ['-I'+str(root/m['source']/'tests')]",
    'test_subsolver_binary':'test_subsolver_core_binary',
    "for name in ('test_subsolver','test_sweep'):":"for name in ():",
    'Layout-safe complete own-arm objects/headers match fixed binary-watch controls and existing SubSolver/Sweep suites. Initial mixed-header6870 diagnostic is superseded for correctness acceptance, not rewritten. External proofs, B/C, runtime, memory and current-source fuzz remain separate.':
        'Additional direct four-pass clausal-core rebuild/shuffle controls agree on complete own-arm objects and headers, with independently replayed derivations against original IDs. Both meters and reference cache-line boundaries included. This supplements6874; does not modify frozen submitted binaries or claim fuzz/runtime acceptance.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
