#!/usr/bin/env python3
"""Check actual old/new ITE-dependent proofs through strong native oracles."""
from itertools import product
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),'--deadline','2026-09-10T06:00:00-04:00'],check=True)
c = root/'benchmarks/runs/20260910-6990-ite-pair-proofs'; assert not c.exists()
(c/'logs').mkdir(parents=True)
hashes = {}; records = []; outputs = {}; removed = 0
def bind(p): hashes[str(p.relative_to(root))] = sha(p)
def run(name, command):
    command = list(map(str,command))
    p = subprocess.run(command,cwd=c,capture_output=True,text=True)
    log = c/'logs'/(name+'.json')
    with log.open('x') as stream:
        json.dump(dict(command=command,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr),stream,indent=2); stream.write('\n')
    bind(log)
    return p
for mode in (0,1):
    cnf = c/f'negative-{mode}.cnf'; proof = cnf.with_suffix('.proof')
    cnf.write_text('p cnf 1 1\n1 0\n'); proof.write_text('2 0 1 0\n' if mode else '0\n')
    checker = root/'tools/checkers/bin'/('cake_lpr' if mode else 'drat-trim')
    p = run('negative-'+str(mode),[checker,cnf,proof])
    assert proof_check_status(checker.name,p.returncode,p.stdout,p.stderr) == 'rejected'
    bind(checker); removed += cnf.stat().st_size + proof.stat().st_size
    cnf.unlink(); proof.unlink()
fixture = root/'benchmarks/probes/scratch/test-ite-pair-proof-6989.cpp'; bind(fixture)
control_path = root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build/control-6985.json'; bind(control_path)
control = json.loads(control_path.read_text())
for rel,digest in control['hashes'].items(): assert sha(root/rel)==digest,rel
for arm, tree_name in (('previous','reduce-watermark-6730'),('candidate','ite-pair-hoist-6969')):
    tree = root/('.claude/worktrees/runtime-'+tree_name)
    objects = [root/p for p in control['hashes'] if '/CMakeFiles/tactix_lib.dir/' in p and p.endswith('.o') and not p.endswith('/congruence/congruence.cpp.o')]
    objects.append(root/f'benchmarks/runs/20260910-6969-ite-pair-hoist-build/{arm}-congruence.o')
    assert len(objects) > 1
    for obj in objects: bind(obj)
    exe = c/(arm+'-fixture')
    p = run(arm+'-build',['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2',
        '-I'+str(tree/'src'),'-I'+str(tree/'include'),fixture,*objects,'-o',exe])
    assert p.returncode == 0,p.stderr
    bind(exe); outputs[arm] = []
    for case in product((0,1,2),range(8),(0,1),(1,2),(0,1)):
        name = arm+'-'+'-'.join(map(str,case)); mode = case[-1]
        cnf = c/(name+'.cnf'); proof = c/(name+'.proof')
        p = run(name+'-fixture',[exe,*case,cnf,proof]); assert p.returncode == 0,(name,p.stderr)
        checker = root/'tools/checkers/bin'/('cake_lpr' if mode else 'drat-trim')
        q = run(name+'-checker',[checker,cnf,proof])
        status = proof_check_status(checker.name,q.returncode,q.stdout,q.stderr)
        assert status == 'verified',(name,status,q.stdout,q.stderr)
        records.append(dict(name=name,arm=arm,case=case,checker=checker.name,status=status,
            input_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size,fixture_stdout=p.stdout))
        outputs[arm].append((p.stdout,sha(cnf),sha(proof),proof.stat().st_size))
        removed += cnf.stat().st_size + proof.stat().st_size
        cnf.unlink(); proof.unlink()
assert len(records) == 384 and outputs['previous'] == outputs['candidate']
assert sum(r['checker']=='cake_lpr' for r in records) == 192
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
for p in (Path(__file__),root/'tools/bench/proof_oracle.py'): bind(p)
with (c/'summary.json').open('x') as stream:
    json.dump(dict(checked=384,LRAT_cake_lpr=192,DRAT_drat_trim=192,negative_controls=2,
        hashes=hashes,records=records,removed_bytes=removed,
        scope='192 proofs per actualold/new Congruence arm: eight shared ITE definitions per output, all condition/branch signs, root-effective3/4/5, duplicates and reversed encounters. Opposite original output roots assigned only after extraction; newly derived equivalence closes the contradiction. Exact cross-arm input/proof/statistics.',
        limitation='Fixed proof witnesses, not fuzz or real-instance runtime acceptance. No promotion.'),stream,indent=2); stream.write('\n')
print('PASS384 proofs;192 cake_lpr/192 DRAT;2 negatives; exact both arms; removed',removed,'bytes')
