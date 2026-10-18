#!/usr/bin/env python3
"""Turn one authenticated Tactix matrix reason into a FRAT-XOR lemma challenge.

This diagnostic proves one reason independently of Tactix's clausal proof.
It accepts only complete source-XOR CNF encodings whose physical clauses are
present in the original input. A source derived by inprocessing is declined;
certifying such a source needs an additional checked proof prefix.
"""
import argparse
from collections import Counter
import hashlib
import json
import lzma
import os
from pathlib import Path
import shutil
import tempfile

INT32_MAX = 2**31 - 1

def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            value.update(block)
    return value.hexdigest()

def cnf_clauses(path):
    opener = lzma.open if path.suffix == '.xz' else open
    with opener(path, 'rt') as stream:
        header = None
        clause = []
        found = 0
        for text in stream:
            stripped = text.lstrip()
            if not stripped or stripped.startswith('c'):
                continue
            if header is None:
                words = stripped.split()
                if len(words) != 4 or words[:2] != ['p', 'cnf']:
                    raise ValueError('invalid original CNF header')
                variables, count = map(int, words[2:])
                if not 0 <= variables <= INT32_MAX or count < 0:
                    raise ValueError('invalid original CNF dimensions')
                header = (variables, count)
                yield ('header', header)
                continue
            for token in stripped.split():
                literal = int(token)
                if not literal:
                    if not clause:
                        raise ValueError('empty original clause unsupported')
                    found += 1
                    yield ('clause', tuple(sorted(clause)))
                    clause = []
                elif abs(literal) > header[0] or abs(literal) == INT32_MAX:
                    raise ValueError('out-of-range original literal')
                else:
                    clause.append(literal)
        if header is None or clause or found != header[1]:
            raise ValueError('original CNF clause count mismatch')

def equation(source, limit):
    variables = source.get('variables')
    rhs = source.get('rhs')
    premises = source.get('premises')
    if (type(variables) is not list or not 1 <= len(variables) <= limit or
            any(type(v) is not int or v <= 0 or v >= INT32_MAX
                for v in variables) or
            variables != sorted(set(variables)) or
            type(rhs) is not int or rhs not in (0, 1) or
            type(premises) is not list):
        raise ValueError('invalid source equation')
    expected = Counter()
    for mask in range(1 << len(variables)):
        if bin(mask).count('1') % 2 != rhs:
            clause = tuple(sorted(-variable if mask & (1 << bit) else variable
                                  for bit, variable in enumerate(variables)))
            expected[clause] += 1
    actual = Counter()
    for premise in premises:
        clause = premise.get('clause') if type(premise) is dict else None
        if (type(clause) is not list or len(clause) != len(variables) or
                any(type(lit) is not int or lit == 0 or
                    abs(lit) >= INT32_MAX for lit in clause) or
                len({abs(lit) for lit in clause}) != len(clause)):
            raise ValueError('invalid source premise')
        actual[tuple(sorted(clause))] += 1
    if actual != expected:
        raise ValueError('source premises are not the complete XOR CNF')
    return variables, rhs, premises

def line(literals):
    return ' '.join(map(str, literals))+' 0'

def signed_xor(variables, rhs):
    signed = variables.copy()
    if not rhs:
        if not signed:
            raise ValueError('false empty XOR row')
        signed[0] = -signed[0]
    return signed

def convert(record, original, output, maximum_sources, maximum_arity,
            record_sha256):
    original_sha256 = digest(original)
    if record.get('kind') != 'affine-matrix-reason':
        raise ValueError('not an affine-matrix-reason record')
    sources = record.get('sources')
    ordinals = record.get('source_ordinals')
    reason = record.get('reason')
    if (type(sources) is not list or
            not 1 <= len(sources) <= maximum_sources or
            type(ordinals) is not list or len(ordinals) != len(sources) or
            [s.get('ordinal') for s in sources] != ordinals or
            len(set(ordinals)) != len(ordinals) or
            type(reason) is not list or not reason):
        raise ValueError('missing source identity or reason')
    rows = []
    clauses = []
    occurrences = {}
    xor_support = set()
    row_rhs = 0
    for source in sources:
        variables, rhs, premises = equation(source, maximum_arity)
        rows.append((variables, rhs, []))
        row_rhs ^= rhs
        for variable in variables:
            if variable in xor_support:
                xor_support.remove(variable)
            else:
                xor_support.add(variable)
        for premise in premises:
            occurrence = premise.get('occurrence')
            clause_id = premise.get('clause_id')
            clause = premise['clause']
            if (type(occurrence) is not int or occurrence <= 0 or
                    type(clause_id) is not int or clause_id <= 0):
                raise ValueError('missing physical premise identity')
            identity = (clause_id, tuple(clause))
            if occurrence in occurrences:
                if occurrences[occurrence][0] != identity:
                    raise ValueError('source occurrence changed identity')
                index = occurrences[occurrence][1]
            else:
                index = len(clauses)+1
                occurrences[occurrence] = (identity, index)
                clauses.append(clause)
            rows[-1][2].append(index)
    if (any(type(lit) is not int or not lit or abs(lit) >= INT32_MAX
            for lit in reason) or
            len({abs(lit) for lit in reason}) != len(reason) or
            {abs(lit) for lit in reason} != xor_support or
            sum(lit < 0 for lit in reason) % 2 == row_rhs):
        raise ValueError('reason is not implied by the source XOR sum')
    needed = Counter(tuple(sorted(clause)) for clause in clauses)
    original_variables = None
    for kind, payload in cnf_clauses(original):
        if kind == 'header':
            original_variables = payload[0]
        elif needed.get(payload, 0):
            needed[payload] -= 1
    if any(needed.values()):
        raise ValueError('source premises are not an original-CNF multiset subset')
    if (original_variables is None or
            original_variables >= INT32_MAX-1 or
            any(abs(lit) > original_variables for lit in reason) or
            any(abs(lit) > original_variables for clause in clauses
                for lit in clause)):
        raise ValueError('original CNF variable range incompatible with source')
    selector = original_variables+1
    challenge = clauses + [[selector]] + [[-selector, -lit] for lit in reason]
    proof = [f'o {index} {line(clause)}'
             for index, clause in enumerate(challenge, 1)]
    next_id = len(challenge)
    source_ids = []
    for variables, rhs, premise_ids in rows:
        next_id += 1
        source_ids.append(next_id)
        proof.append(f'i x {next_id} {line(signed_xor(variables, rhs))} '
                     f'l {line(premise_ids)}')
    row_variables = sorted(xor_support)
    if len(source_ids) > 1:
        next_id += 1
        row_id = next_id
        proof.append(f'a x {row_id} {line(signed_xor(row_variables, row_rhs))} '
                     f'l {line(source_ids)}')
    else:
        row_id = source_ids[0]
    next_id += 1
    reason_id = next_id
    proof.append(f'i {reason_id} {line(reason)} l {row_id} 0')
    next_id += 1
    negated_selector_id = next_id
    negated_reason_ids = list(range(len(clauses)+2, len(challenge)+1))
    proof.append(f'a {negated_selector_id} {-selector} 0 l '
                 f'{line([reason_id, *negated_reason_ids])}')
    next_id += 1
    proof.append(f'a {next_id} 0 l {len(clauses)+1} {negated_selector_id} 0')
    proof += [f'f {next_id} 0',
              f'f {negated_selector_id} {-selector} 0',
              f'f {reason_id} {line(reason)}']
    if len(source_ids) > 1:
        proof.append(f'f x {row_id} {line(signed_xor(row_variables, row_rhs))}')
    for (variables, rhs, _), xid in reversed(list(zip(rows, source_ids))):
        proof.append(f'f x {xid} {line(signed_xor(variables, rhs))}')
    proof += [f'f {index} {line(clause)}'
              for index, clause in enumerate(challenge, 1)]

    if output.exists():
        raise FileExistsError(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=output.name+'.staging-',
                                    dir=output.parent))
    try:
        (staging/'source.cnf').write_text(
            f'p cnf {original_variables} {len(clauses)}\n'+
            ''.join(line(clause)+'\n' for clause in clauses))
        (staging/'challenge.cnf').write_text(
            f'p cnf {selector} {len(challenge)}\n'+
            ''.join(line(clause)+'\n' for clause in challenge))
        (staging/'lemmas.txt').write_text(line(reason)+'\n')
        (staging/'challenge.frat').write_text('\n'.join(proof)+'\n')
        if digest(original) != original_sha256:
            raise RuntimeError('original CNF changed during conversion')
        summary = {'schema':'tactix-affine-record-xor-challenge-v1',
                   'original_sha256':original_sha256,
                   'record_sha256':record_sha256,
                   'source_clauses':len(clauses),
                   'source_equations':len(rows),
                   'reason_width':len(reason),
                   'challenge_clauses':len(challenge),
                   'record_action_id':record.get('action_id'),
                   'record_semantic_id':record.get('semantic_id'),
                   'source_occurrences':len(occurrences)}
        (staging/'summary.json').write_text(json.dumps(summary,indent=2,
                                                   sort_keys=True)+'\n')
        os.replace(staging, output)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
    return summary

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original', type=Path, required=True)
    parser.add_argument('--record', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--maximum-sources', type=int, default=4096)
    parser.add_argument('--maximum-arity', type=int, default=8)
    args = parser.parse_args()
    if (not 1 <= args.maximum_sources <= 4096 or
            not 1 <= args.maximum_arity <= 8):
        parser.error('invalid resource limits')
    record_bytes = args.record.read_bytes()
    record = json.loads(record_bytes)
    print(json.dumps(convert(record, args.original, args.output,
                             args.maximum_sources, args.maximum_arity,
                             hashlib.sha256(record_bytes).hexdigest()),
                     sort_keys=True))

if __name__ == '__main__':
    main()
