#!/usr/bin/env python3
"""Same immutable public-rule oracle, now physical width11/effective width3."""
from pathlib import Path
source = Path(__file__).with_name('check-padded-xor-6526.py')
code = source.read_text()
for old, new in {
    '20260909-6526-padded-xor-control': '20260909-6538-wide-padded-xor-control',
    'test-padded-xor-6525.cpp': 'test-wide-padded-xor-6537.cpp',
    "assert not c.exists(); c.mkdir()":
        "assert c.is_dir() and {p.name for p in c.iterdir()} == {'build.log'}; # failed compile only; no solver/proof artifacts",
    "(c/'build.log').open('x')": "(c/'build-2.log').open('x')",
    'Fixed five-variable public-rule tests, eight exhaustive32-assignment SAT controls and eight independently checked UNSAT proofs; not fuzz or runtime evidence. No source change or native omission copied.':
        'Fixed twelve-variable public-rule tests with eight false padding literals: physical width11, root-effective width3. Sixteen pure-extraction controls, eight exhaustive4096-assignment SAT model-set controls and eight independently checked UNSAT proofs. Not fuzz, runtime evidence, an early-exit implementation or source change.',
}.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
