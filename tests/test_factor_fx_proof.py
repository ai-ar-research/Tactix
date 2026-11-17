"""Small independent proofs/models for actual XOR/ITE clause replacement."""
from itertools import product
from pathlib import Path
import os
import sys
from proof_test_support import Fixture, remove_clause

fixture, root = map(Path, sys.argv[1:])
f = Fixture(root, 'fx-factoring')
cases = [(gate, signs, sat, 'input')
         for gate, signs, sat in product(('xor', 'ite'), (0, 7), (False, True))]
cases += [(gate, 7, sat, scope)
          for gate, sat, scope in product(('xor', 'ite'), (False, True), ('learned', 'mixed'))]
cases += [(gate, 7, sat, 'zero') for gate, sat in product(('xor', 'ite'), (False, True))]
for native in (False, True):
    for gate, signs, sat, scope in cases:
        key = gate, signs, sat, scope, native
        stem = f.fresh(f'{gate}-{signs}-{sat}-{scope}-{native}')
        args = [fixture, stem, gate, 'sat' if sat else 'unsat', signs, scope]
        if native:
            args.append('native')
        f.run(key, *args)
        q = f.query(stem, 'dsr', sat, key, audit='dsr')
        if not sat:
            wrong = f.fresh('missing.cnf')
            remove_clause(q.cnf, wrong)
            f.reject(q.kind, q.route, wrong, q.proof, errors=('claims contradiction',))
# A small CI sample; large seeded campaigns use the same native fixture and
# independent oracles outside CI rather than adding slow registered tests.
for case in range(int(os.environ.get('TACTIX_FX_FUZZ_CASES', '4'))):
    seed = 5 * case
    gate = ('xor', 'ite')[seed % 2]
    sat = bool((seed // 2) % 2)
    scope = ('input', 'learned', 'mixed', 'zero')[(seed // 4) % 4]
    key = 'random', seed, gate, sat, scope
    stem = f.fresh(f'random-{seed}')
    f.run(key, fixture, stem, gate, 'sat' if sat else 'unsat',
          (seed // 16) % 8, scope, 'native', seed)
    f.query(stem, 'dsr', sat, key, audit='dsr')
f.report('FX factoring', 'actual replacements independently checked')
