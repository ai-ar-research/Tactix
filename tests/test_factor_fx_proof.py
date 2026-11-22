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
native_cases = [(*case, True) for case in cases]
public_cases = [(gate, 7, sat, 'mixed', False)
                for gate, sat in product(('xor', 'ite'), (False, True))]
for gate, signs, sat, scope, native in native_cases + public_cases:
    key = gate, signs, sat, scope, native
    stem = f.fresh(f'{gate}-{signs}-{sat}-{scope}-{native}')
    args = [fixture, stem, gate, 'sat' if sat else 'unsat', signs, scope]
    if native:
        args.append('native')
    f.run(key, *args)
    q = f.query(stem, 'dsr', sat, key, audit='dsr')
    if native and signs == 7 and not sat and scope == 'input':
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
for gate, sat, scope in [('xor', True, 'input'), ('ite', False, 'learned')]:
    key = 'bounded', gate, sat, scope
    stem = f.fresh(f'bounded-{gate}')
    f.run(key, fixture, stem, gate, 'sat' if sat else 'unsat', 7, scope, 'bounded', 31)
    f.query(stem, 'dsr', sat, key, audit='dsr')
for gate, sat in [('xor', True), ('ite', False)]:
    key = 'dense', gate, sat
    stem = f.fresh(f'dense-{gate}')
    f.run(key, fixture, stem, gate, 'sat' if sat else 'unsat', 7, 'mixed', 'dense', 31)
    f.query(stem, 'dsr', sat, key, audit='dsr')
f.report('FX factoring', 'actual replacements independently checked')
