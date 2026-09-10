#!/usr/bin/env python3
"""Check actual old/new Factor fixed certificates with strong negative controls."""
from pathlib import Path
from itertools import product
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),'--deadline','2026-09-10T06:00:00-04:00'],check=True)
c = root/'benchmarks/runs/20260910-6932-factor-count-proofs'
assert not c.exists()
(c/'logs').mkdir(parents=True)
hashes = {}; records = []; outputs = {}; removed = 0
def bind(p): hashes[str(p.relative_to(root))] = sha(p)
def run(name,command):
    command = list(map(str,command))
    p = subprocess.run(command,cwd=c,capture_output=True,text=True)
    log = c/'logs'/(name+'.json')
    with log.open('x') as stream:
        json.dump(dict(command=command,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr),stream,indent=2)
        stream.write('\n')
    bind(log)
    return p
for mode in (0,1):
    cnf = c/f'negative-{mode}.cnf'; proof = cnf.with_suffix('.proof')
    cnf.write_text('p cnf 1 1\n1 0\n'); proof.write_text('2 0 1 0\n' if mode else '0\n')
    checker = root/'tools/checkers/bin'/('cake_lpr' if mode else 'drat-trim')
    p = run('negative-'+str(mode),[checker,cnf,proof])
    assert proof_check_status(checker.name,p.returncode,p.stdout,p.stderr) == 'rejected'
    bind(checker)
    removed += cnf.stat().st_size + proof.stat().st_size
    cnf.unlink(); proof.unlink()
fixture = root/'benchmarks/probes/scratch/test-factor-count-proof-6931.cpp'; bind(fixture)
control_path = root/'benchmarks/runs/20260910-6925-factor-count-build/control-6928.json'; bind(control_path)
control = json.loads(control_path.read_text())
for group in ('hashes','parent_objects','artifacts','logs'):
    for rel,digest in control[group].items(): assert sha(root/rel)==digest,rel
for arm, tree_name in (('previous','reduce-watermark-6730'),('candidate','factor-count-6925')):
    tree = root/('.claude/worktrees/runtime-'+tree_name)
    pin = root/'build/pin-6730-reduce-watermark-release'
    objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
    objects = [p for p in objects if not str(p).endswith('/factor/factor.cpp.o')]
    objects.append(root/f'benchmarks/runs/20260910-6925-factor-count-build/{arm}-factor-6928.o')
    for obj in objects: bind(obj)
    exe = c/(arm+'-fixture')
    p = run(arm+'-build',['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2',
        '-I'+str(tree/'src'),'-I'+str(tree/'include'),fixture,*objects,'-o',exe])
    assert p.returncode == 0,p.stderr
    bind(exe); outputs[arm] = []
    for case in product((3,9,33),(3,8),(1,2,4),(0,1),(0,1),(0,1)):
        name = arm+'-'+'-'.join(map(str,case)); mode = case[-1]
        cnf = c/(name+'.cnf'); proof = c/(name+'.proof')
        p = run(name+'-fixture',[exe,*case,cnf,proof])
        assert p.returncode == 0,(name,p.stderr)
        checker = root/'tools/checkers/bin'/('cake_lpr' if mode else 'drat-trim')
        q = run(name+'-checker',[checker,cnf,proof])
        status = proof_check_status(checker.name,q.returncode,q.stdout,q.stderr)
        assert status == 'verified',(name,status,q.stdout,q.stderr)
        record = dict(name=name,arm=arm,case=case,checker=checker.name,status=status,
            input_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size,fixture_stdout=p.stdout)
        records.append(record)
        outputs[arm].append((p.stdout,sha(cnf),sha(proof),proof.stat().st_size))
        removed += cnf.stat().st_size + proof.stat().st_size
        cnf.unlink(); proof.unlink()
assert len(records) == 288 and outputs['previous'] == outputs['candidate']
assert sum(r['checker']=='cake_lpr' for r in records) == 144
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
for p in (Path(__file__),root/'tools/bench/proof_oracle.py',root/'tools/bench/test_proof_oracle.py',root/'tools/bench/test_proof_oracle_binaries.py'): bind(p)
with (c/'summary.json').open('x') as stream:
    json.dump(dict(checked=288,LRAT_cake_lpr=144,DRAT_drat_trim=144,negative_controls=2,
        hashes=hashes,records=records,removed_bytes=removed,
        scope='144 fixed native proofs per actual6730/6925 Factor arm; full grids and budget cuts,2 encounter orders,3 widths,3 row counts,2 column counts. Unlimited cases introduce a fresh variable before original input roots force a generated-quotient contradiction. Exact cross-arm input/certificate/state; no historical legacy-checker dependence.',
        limitation='Actual Factor object with hash-identical other6730 sources; independent full builds and real-instance B/C/runtime proofs still required. No source fuzz, broad competitiveness or promotion.'),stream,indent=2)
    stream.write('\n')
print('PASS288 fixed proofs;144 cake_lpr/144 DRAT;2 negatives; exact both arms; removed',removed,'bytes')
