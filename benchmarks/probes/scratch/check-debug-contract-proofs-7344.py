#!/usr/bin/env python3
"""Independently check fixed Substitute, disconnected collection and C2 proofs."""
import itertools
import json
import os
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
authority = root / 'benchmarks/runs/20260911-7340-debug-contracts-build/manifest.json'
build = json.loads(authority.read_text()); assert build['status'] == 'PASS'
source = root / build['source']
for rel, digest in build['files'].items(): assert sha(source / rel) == digest, rel
campaign = root / 'benchmarks/runs/20260911-7344-debug-contract-proofs'
assert not campaign.exists()
(campaign / 'logs').mkdir(parents=True)
hashes = {str(authority.relative_to(root)): sha(authority)}
records, forward = [], []
removed = 0
driver = Path(__file__).with_name('debug-contract-proofs-7343.cpp')
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
def run(name, command):
    command = list(map(str, command))
    result = subprocess.run(command, cwd=campaign, env=env, capture_output=True, text=True)
    path = campaign / 'logs' / (name + '.json')
    with path.open('x') as out:
        json.dump(dict(command=command, returncode=result.returncode,
                       stdout=result.stdout, stderr=result.stderr), out, indent=2)
    hashes[str(path.relative_to(root))] = sha(path)
    return result
def consume(path):
    global removed
    assert path.is_relative_to(campaign) and path.suffix in ('.cnf','.proof','.out')
    removed += path.stat().st_size
    path.unlink()
def check(name, cnf, proof, mode, result, expected):
    assert result.returncode == (20 if expected else 10), (name,result.stdout,result.stderr)
    if expected:
        checker = root / 'tools/checkers/bin' / ('cake_lpr' if mode == 'lrat' else 'drat-trim')
        checked = run(name + '-checker', [checker, cnf, proof])
        assert proof_check_status(checker.name, checked.returncode, checked.stdout, checked.stderr) == 'verified', (name,checked.stdout,checked.stderr)
    else:
        model = cnf.with_suffix('.out')
        with model.open('x') as out: out.write(result.stdout)
        checker = root / 'benchmarks/runs/20260911-7327-rank25-root-gdb/pins/checkmodel'
        checked = run(name + '-checker', [checker, cnf, model])
        assert checked.returncode == 0 and checked.stdout.startswith('checkmodel: OK'), (name,checked.stdout,checked.stderr)
        consume(model)
    hashes[str(checker.relative_to(root))] = sha(checker)
    return dict(name=name, unsat=expected, cnf_sha256=sha(cnf), proof_sha256=sha(proof), proof_bytes=proof.stat().st_size)
for kind in ('release','sanitizers'):
    pin = root / ('build/pin-7340-debug-contracts-' + kind)
    objects = sorted((pin / 'CMakeFiles/tactix_lib.dir').rglob('*.o')); assert objects
    binary = campaign / (kind + '-proof-driver')
    flags = ['-O3','-DNDEBUG'] if kind == 'release' else ['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
    result = run(kind + '-build', ['/cm/local/apps/gcc/13.1.0/bin/g++','-std=c++20',
                 '-fno-exceptions','-fno-rtti',*flags,'-I'+str(source/'src'),'-I'+str(source/'include'),
                 driver,*objects,'-o',binary])
    assert result.returncode == 0, (result.stdout,result.stderr)
    for path in [binary,*objects]: hashes[str(path.relative_to(root))] = sha(path)
    cases = []
    for copies, sign, wide, action in itertools.product((1,3), (1,-1), (False,True), ('canonical','explicit')):
        clauses = [[-sign,2],[sign,-2]] * copies + [[sign,2,3,4] if wide else [sign,2,3]]
        cases.append(('substitute',action,clauses))
    for sign, action in itertools.product((1,-1), ('indexed','unindexed')):
        cases.append(('collect',action,[[sign],[sign,4,5],[4,5,6],[-4,7]]))
    for ordinal,(operation,action,base) in enumerate(cases):
        for unsat, mode in itertools.product((False,True), ('drat','lrat')):
            clauses = base + [[9,10],[-9,10],[9,-10]] + ([[-9,-10]] if unsat else [])
            name = f'{kind}-{ordinal}-{int(unsat)}-{mode}'
            cnf,proof = campaign/(name+'.cnf'),campaign/(name+'.proof')
            with cnf.open('x') as out:
                out.write(f'p cnf 10 {len(clauses)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in clauses))
            result = run(name+'-fixture',[binary,cnf,proof,mode,operation,action])
            record = check(name,cnf,proof,mode,result,unsat)
            assert f'c reached {operation} ' in result.stdout
            records.append(record); consume(cnf); consume(proof)
    folder = campaign / (kind + '-forward'); folder.mkdir()
    test = pin / 'tests/test_forward_unit'
    hashes[str(test.relative_to(root))] = sha(test)
    result = run(kind+'-forward-fixture',[test,folder])
    assert result.returncode == 0,(result.stdout,result.stderr)
    inputs = sorted(folder.glob('*.cnf')); assert len(inputs) == 16
    for cnf in inputs:
        proof = cnf.with_suffix('.proof')
        mode = 'lrat' if cnf.stem.endswith('-lrat') else 'drat'
        checker = root / 'tools/checkers/bin' / ('cake_lpr' if mode=='lrat' else 'drat-trim')
        result = run(kind+'-'+cnf.stem+'-checker',[checker,cnf,proof])
        assert proof_check_status(checker.name,result.returncode,result.stdout,result.stderr)=='verified',(cnf,result.stdout,result.stderr)
        forward.append(dict(name=kind+'-'+cnf.stem, cnf_sha256=sha(cnf),proof_sha256=sha(proof)))
        consume(cnf); consume(proof)
    print(kind,'Substitute/collection/C2 proofs and models PASS',flush=True)
assert len(records)==160 and len(forward)==32
for path in [Path(__file__),driver,root/'tools/bench/proof_oracle.py',
             root/'benchmarks/runs/20260911-7327-rank25-root-gdb/pins/modelcheck.py']:
    hashes[str(path.relative_to(root))] = sha(path)
assert not any(campaign.rglob('*.proof')) and not any(campaign.rglob('*.cnf'))
with (campaign/'manifest.json').open('x') as out:
    json.dump(dict(status='PASS',hashes=hashes,records=records,forward=forward,
                   checked_UNSAT=112,checked_SAT=80,removed_bytes=removed,
                   scope='Actual properly named canonical/explicit Substitute, disconnected GC with/without occurrence cache, both root signs and clause widths/multiplicity, and every retained C2 full-proof fixture; Release and unsuppressed Debug sanitizers. Original CNFs and unchanged independent checkers.'),out,indent=2)
