#!/usr/bin/env python3
"""Check globally entailed XOR lemmas, then a DSR proof using those lemmas.

The algebraic FRAT proof is checked against a selector challenge built from
an authenticated subset of the original CNF. The DSR proof is checked against
the original CNF with exactly the same lemmas appended as input clauses.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

INT32_MAX = 2**31 - 1

def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            digest.update(block)
    return digest.hexdigest()

def parse_clause(text, variables):
    words = text.split()
    if not words or words[-1] != '0':
        raise ValueError('clause must end with 0')
    literals = [int(word) for word in words[:-1]]
    if not literals or any(not lit or abs(lit) > variables for lit in literals):
        raise ValueError('empty or out-of-range clause')
    if len({abs(lit) for lit in literals}) != len(literals):
        raise ValueError('duplicate or tautological variable in clause')
    return literals

def read_cnf(path):
    header = None
    clauses = []
    with path.open() as stream:
        for text in stream:
            text = text.strip()
            if not text or text.startswith('c'):
                continue
            if header is None:
                words = text.split()
                if len(words) != 4 or words[:2] != ['p','cnf']:
                    raise ValueError(f'{path}: invalid CNF header')
                variables, count = map(int, words[2:])
                if variables < 0 or variables > INT32_MAX or count < 0:
                    raise ValueError(f'{path}: invalid CNF dimensions')
                header = variables, count
            else:
                clauses.append(parse_clause(text, header[0]))
    if header is None or len(clauses) != header[1]:
        raise ValueError(f'{path}: clause count mismatch')
    return header[0], clauses

def scan_original(path, source_counts):
    """Validate a potentially large original CNF without retaining its clauses."""
    header = None
    found = 0
    with path.open() as stream:
        for text in stream:
            text = text.strip()
            if not text or text.startswith('c'):
                continue
            if header is None:
                words = text.split()
                if len(words) != 4 or words[:2] != ['p','cnf']:
                    raise ValueError(f'{path}: invalid CNF header')
                variables, count = map(int, words[2:])
                if variables < 0 or variables > INT32_MAX or count < 0:
                    raise ValueError(f'{path}: invalid CNF dimensions')
                header = variables, count
            else:
                clause = parse_clause(text, header[0])
                key = tuple(sorted(clause))
                if source_counts.get(key, 0):
                    source_counts[key] -= 1
                found += 1
    if header is None or found != header[1]:
        raise ValueError(f'{path}: clause count mismatch')
    return header

def read_lemmas(path, variables):
    with path.open() as stream:
        lemmas = [parse_clause(text.strip(), variables) for text in stream
                  if text.strip() and not text.startswith('c')]
    if not lemmas or len(lemmas) > INT32_MAX - variables:
        raise ValueError('invalid lemma count or selector range')
    if len({tuple(sorted(clause)) for clause in lemmas}) != len(lemmas):
        raise ValueError('duplicate lemma')
    return lemmas

def write_cnf(path, variables, clauses):
    with path.open('x') as stream:
        stream.write(f'p cnf {variables} {len(clauses)}\n')
        for clause in clauses:
            stream.write(' '.join(map(str, clause)) + ' 0\n')

def write_augmented(path, original, variables, count, lemmas):
    with path.open('x') as target:
        target.write(f'p cnf {variables} {count+len(lemmas)}\n')
        observed = 0
        header_seen = False
        with original.open() as source:
            for text in source:
                text = text.strip()
                if not text or text.startswith('c'):
                    continue
                if not header_seen:
                    if text.split() != ['p','cnf',str(variables),str(count)]:
                        raise ValueError('original CNF header changed while augmenting')
                    header_seen = True
                    continue
                clause = parse_clause(text, variables)
                target.write(' '.join(map(str, clause))+' 0\n')
                observed += 1
        if not header_seen or observed != count:
            raise ValueError('original CNF changed while augmenting')
        for clause in lemmas:
            target.write(' '.join(map(str, clause))+' 0\n')

def checked_run(command, timeout, env, marker):
    done = subprocess.run([str(part) for part in command], capture_output=True,
                          text=True, timeout=timeout, env=env)
    if done.returncode or marker not in done.stdout.splitlines():
        raise RuntimeError(f'checker failed: {command[0]}: '
                           f'exit={done.returncode}; '
                           f'stdout={done.stdout[-500:]}; '
                           f'stderr={done.stderr[-500:]}')
    return done.stdout.strip()

def verify(args):
    stable_paths = {'original':args.original,'source':args.source,
                    'lemmas':args.lemmas,'frat':args.frat,'dsr':args.dsr,
                    'frat_xor':args.frat_xor,'cake_xlrup':args.cake_xlrup,
                    'proof_oracle':args.proof_oracle,
                    'dsr_trim':args.dsr_trim,
                    'dsr_compress':args.dsr_compress,
                    'srcheck':args.srcheck}
    pinned = {name:sha(path) for name,path in stable_paths.items()}
    source_vars, source = read_cnf(args.source)
    remaining = Counter(tuple(sorted(clause)) for clause in source)
    original_vars, original_count = scan_original(args.original, remaining)
    if source_vars > original_vars:
        raise ValueError('lemma source declares variables outside original CNF')
    if any(remaining.values()):
        raise ValueError('lemma source is not a clause-multiset subset of original')
    lemmas = read_lemmas(args.lemmas, original_vars)
    if args.max_lemmas and len(lemmas) > args.max_lemmas:
        raise ValueError('lemma count exceeds bound')
    selectors = list(range(original_vars+1, original_vars+len(lemmas)+1))
    challenge = source + [selectors]
    challenge += [[-selector,-literal] for selector,clause in zip(selectors,lemmas)
                  for literal in clause]
    with tempfile.TemporaryDirectory(prefix='tactix-xor-dsr-',dir=args.scratch) as temp:
        work = Path(temp)
        challenge_path, augmented_path = work/'challenge.cnf', work/'augmented.cnf'
        xlrup_path = work/'challenge.xlrup'
        write_cnf(challenge_path, original_vars+len(lemmas), challenge)
        write_augmented(augmented_path, args.original,
                        original_vars, original_count, lemmas)
        env = {**os.environ, 'TMPDIR': temp,
               'FRAT_XOR_TEMP_PATH': str(work/'frat-xor-elab.temp'),
               'CML_HEAP_SIZE': str(args.cake_heap_mib)}
        elaborated = subprocess.run([str(args.frat_xor),'elab',str(args.frat),
            str(challenge_path),str(xlrup_path)],capture_output=True,text=True,
            timeout=args.timeout,env=env)
        if elaborated.returncode:
            raise RuntimeError('FRAT-XOR elaboration failed: '+elaborated.stderr[-1000:])
        cake_output = checked_run([args.cake_xlrup,challenge_path,xlrup_path],
                                  args.timeout,env,'s VERIFIED UNSAT')
        dsr_output = checked_run([sys.executable,args.proof_oracle,'check-dsr',
            augmented_path,args.dsr,'--dsr-trim',args.dsr_trim,
            '--dsr-compress',args.dsr_compress,'--srcheck',args.srcheck,
            '--every-lemma'],args.timeout,env,'s VERIFIED UNSAT')
        if any(sha(path) != pinned[name] for name,path in stable_paths.items()):
            raise RuntimeError('input or checker changed during composition')
        result = {'schema':'tactix-xor-dsr-composition-v1',
                  'composer_sha256':sha(Path(__file__).resolve()),
                  'source_clauses':len(source),'original_clauses':original_count,
                  'lemmas':len(lemmas),'challenge_clauses':len(challenge),
                  'augmented_clauses':original_count+len(lemmas),
                  'source_subset_checked':True,'selector_binding_checked':True,
                  'cake_verified':True,'every_lemma_dsr_verified':True,
                  'cake_stdout':cake_output,'dsr_stdout':dsr_output,
                  'sha256':{**pinned,'challenge':sha(challenge_path),
                            'augmented':sha(augmented_path),
                            'xlrup':sha(xlrup_path)},
                  'raw_intermediates_deleted':True}
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('original','source','lemmas','frat','dsr','frat-xor',
                 'cake-xlrup','proof-oracle','dsr-trim','dsr-compress','srcheck'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--scratch',type=Path,default=Path('/tmp'))
    parser.add_argument('--timeout',type=int,default=120)
    parser.add_argument('--cake-heap-mib',type=int,default=4096)
    parser.add_argument('--max-lemmas',type=int,default=0)
    parser.add_argument('--report',type=Path)
    args=parser.parse_args()
    if args.timeout <= 0 or args.cake_heap_mib < 1024 or args.max_lemmas < 0:
        parser.error('invalid resource bound')
    result=verify(args)
    if args.report:
        with args.report.open('x') as output:
            json.dump(result,output,indent=2,sort_keys=True)
            output.write('\n')
    print('s VERIFIED XOR-LEMMA/DSR COMPOSITION')

if __name__=='__main__':
    main()
