#!/usr/bin/env python3
"""Check an original-CNF XOR refutation on a verified CMS Tier-D UNSAT row."""
import argparse
from collections import Counter, defaultdict
from itertools import product
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
from affine_record_xor_challenge import cnf_clauses, digest, signed_xor
from cms_tierd_cpuq import independently_checked

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--campaign', type=Path, required=True)
parser.add_argument('--task', type=int, required=True)
parser.add_argument('--report', type=Path, required=True)
parser.add_argument('--expect-equations', type=int)
parser.add_argument('--expect-selected', type=int)
parser.add_argument('--expect-clauses', type=int)
parser.add_argument('--maximum-equations', type=int, default=10000)
parser.add_argument('--maximum-clauses', type=int, default=100000)
args = parser.parse_args()
if (args.task < 0 or args.maximum_equations <= 0 or
        args.maximum_clauses <= 0):
    parser.error('invalid resource limit or task')
cms_campaign = args.campaign.resolve()
cms_result = cms_campaign / 'results' / f'{args.task:05d}.json'
result = json.loads(cms_result.read_text())
manifest = json.loads((cms_campaign / 'manifest.json').read_text())
if (result.get('task') != args.task or
        not independently_checked(result, manifest) or
        result.get('verdict') != 'UNSAT' or
        result['solve']['wall_seconds'] > 600 or
        result.get('scratch_deleted') is not True):
    raise RuntimeError('CMS row is not an independently checked <=600 s UNSAT')
original = root / 'benchmarks/cache' / result['instance']
original_sha = digest(original)
pins = cms_campaign / 'pins'
for name in ('frat_xor', 'cake_xlrup'):
    if digest(pins / name) != manifest['provenance'][name.replace('_', '-')]['sha256']:
        raise RuntimeError('pinned checker changed')

clauses = []
patterns = defaultdict(dict)
variables = None
for kind, item in cnf_clauses(original):
    if kind == 'header':
        variables = item[0]
        continue
    clauses.append(list(item))
    support = tuple(sorted(abs(lit) for lit in item))
    if len(support) != len(set(support)) or len(support) > 8:
        continue
    bits = tuple(int(-v in item) for v in support)
    patterns[support].setdefault(bits, len(clauses))
if variables is None or len(clauses) > args.maximum_clauses:
    raise RuntimeError('original input exceeds declared clause limit')

equations = []
for support, available in patterns.items():
    for rhs in (0, 1):
        forbidden = [bits for bits in product((0, 1), repeat=len(support))
                     if sum(bits) % 2 != rhs]
        if all(bits in available for bits in forbidden):
            equations.append((support, rhs, [available[bits] for bits in forbidden]))
if (len(equations) > args.maximum_equations or
        (args.expect_equations is not None and
         len(equations) != args.expect_equations)):
    raise RuntimeError('original XOR census changed or exceeded limit')

basis = {}
selection = None
for index, (support, rhs, _) in enumerate(equations):
    residue = sum(1 << variable for variable in support) | rhs
    combination = 1 << index
    while residue:
        pivot = residue.bit_length() - 1
        if pivot not in basis:
            basis[pivot] = residue, combination
            break
        residue ^= basis[pivot][0]
        combination ^= basis[pivot][1]
    if residue == 1:
        selection = combination
        break
if selection is None:
    raise RuntimeError('original XORs did not yield contradiction')
selected = [row for index, row in enumerate(equations) if (selection >> index) & 1]
if args.expect_selected is not None and len(selected) != args.expect_selected:
    raise RuntimeError('deterministic contradictory subset changed')
support = set()
rhs = 0
for variables_in_row, parity, _ in selected:
    support.symmetric_difference_update(variables_in_row)
    rhs ^= parity
if support or rhs != 1:
    raise RuntimeError('selected XOR equations do not sum to false')

source_ids = []
seen = set()
for _, _, premise_ids in selected:
    for clause_id in premise_ids:
        if clause_id not in seen:
            seen.add(clause_id)
            source_ids.append(clause_id)
local = {clause_id: index + 1 for index, clause_id in enumerate(source_ids)}
if args.expect_clauses is not None and len(source_ids) != args.expect_clauses:
    raise RuntimeError('selected original-clause population changed')

def line(literals):
    return ' '.join(map(str, literals)) + ' 0'

proof = [f'o {local[clause_id]} {line(clauses[clause_id-1])}'
         for clause_id in source_ids]
next_id = len(source_ids)
xids = []
for variables_in_row, parity, premise_ids in selected:
    next_id += 1
    xids.append(next_id)
    proof.append(f'i x {next_id} {line(signed_xor(list(variables_in_row), parity))} '
                 f'l {line([local[clause_id] for clause_id in premise_ids])}')
next_id += 1
contradiction_xid = next_id
contradiction_step = f'a x {contradiction_xid} 0 l {line(xids)}'
proof.append(contradiction_step)
next_id += 1
empty_id = next_id
proof.append(f'i {empty_id} 0 l {contradiction_xid} 0')
proof += [f'f {empty_id} 0', f'f x {contradiction_xid} 0']
for (variables_in_row, parity, _), xid in reversed(list(zip(selected, xids))):
    proof.append(f'f x {xid} {line(signed_xor(list(variables_in_row), parity))}')
proof += [f'f {local[clause_id]} {line(clauses[clause_id-1])}'
          for clause_id in reversed(source_ids)]

with tempfile.TemporaryDirectory(prefix='tactix-lightsout-original-xor-', dir='/tmp') as tmp:
    temp = Path(tmp)
    cnf = temp / 'source.cnf'
    frat = temp / 'proof.frat'
    xlrup = temp / 'proof.xlrup'
    cnf.write_text(f'p cnf {variables} {len(source_ids)}\n' +
                   ''.join(line(clauses[clause_id-1])+'\n' for clause_id in source_ids))
    frat.write_text('\n'.join(proof)+'\n')
    env = {**os.environ, 'FRAT_XOR_TEMP_PATH': str(temp / 'elab.temp'),
           'CML_HEAP_SIZE': '4096', 'TMPDIR': str(temp)}
    elaborated = subprocess.run([str(pins / 'frat_xor'), 'elab', str(frat),
                                 str(cnf), str(xlrup)],
                                capture_output=True, text=True, env=env, timeout=120)
    if elaborated.returncode:
        raise RuntimeError('FRAT-XOR elaboration failed: ' + elaborated.stderr[-500:])
    checked = subprocess.run([str(pins / 'cake_xlrup'), str(cnf), str(xlrup)],
                             capture_output=True, text=True, env=env, timeout=120)
    if checked.returncode or checked.stdout.strip() != 's VERIFIED UNSAT':
        raise RuntimeError('CakeXLRUP rejected lightsout XOR refutation: ' +
                           checked.stderr[-500:])
    wrong_frat = temp / 'wrong-proof.frat'
    wrong_xlrup = temp / 'wrong-proof.xlrup'
    wrong_step = f'a x {contradiction_xid} 0 l {line(xids[1:])}'
    if proof.count(contradiction_step) != 1:
        raise RuntimeError('contradiction step identity changed')
    wrong_frat.write_text('\n'.join(wrong_step if step == contradiction_step else step
                                for step in proof) + '\n')
    wrong_elab = subprocess.run([str(pins / 'frat_xor'), 'elab', str(wrong_frat),
                                 str(cnf), str(wrong_xlrup)],
                                capture_output=True, text=True, env=env, timeout=120)
    wrong_cake = None
    if wrong_elab.returncode == 0:
        wrong_cake = subprocess.run([str(pins / 'cake_xlrup'), str(cnf),
                                     str(wrong_xlrup)],
                                    capture_output=True, text=True, env=env,
                                    timeout=120)
        if wrong_cake.stdout.strip() == 's VERIFIED UNSAT':
            raise RuntimeError('pinned checker accepted wrong XOR sum')
    evidence = {
        'schema': 'tactix-cms-original-xor-refutation-v1',
        'task': args.task, 'instance': result['instance'],
        'cms_result_sha256': digest(cms_result),
        'original_sha256': original_sha,
        'original_variables': variables,
        'original_clauses': len(clauses),
        'original_xor_arities': dict(Counter(len(row[0]) for row in equations)),
        'frat_xor_sha256': digest(pins / 'frat_xor'),
        'cake_xlrup_sha256': digest(pins / 'cake_xlrup'),
        'original_xor_equations': len(equations),
        'selected_contradictory_equations': len(selected),
        'selected_original_clauses': len(source_ids),
        'frat_sha256': digest(frat), 'frat_bytes': frat.stat().st_size,
        'xlrup_sha256': digest(xlrup), 'xlrup_bytes': xlrup.stat().st_size,
        'cake_verified': True, 'cake_stdout': checked.stdout.strip(),
        'wrong_sum_control': {
            'removed_source_xor': xids[0],
            'frat_sha256': digest(wrong_frat),
            'elab_exit': wrong_elab.returncode,
            'elab_stderr_prefix': wrong_elab.stderr[:220],
            'cake_verified': False,
            'cake_exit': wrong_cake.returncode if wrong_cake else None,
            'cake_stderr_prefix': wrong_cake.stderr[:220] if wrong_cake else None,
        },
        'raw_intermediates_deleted': True,
    }
if digest(original) != original_sha:
    raise RuntimeError('original input changed during proof check')
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(
    json.dumps(evidence, indent=2, sort_keys=True) + '\n')
print(f"CMS task {args.task} original XOR contradiction verified from {len(selected)} equations")
