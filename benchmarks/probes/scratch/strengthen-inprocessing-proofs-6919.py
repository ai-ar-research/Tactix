#!/usr/bin/env python3
"""Recheck actual6730/6902 fixed Sweep and Vivify proofs with strong oracles."""
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
c = root/'benchmarks/runs/20260910-6919-strong-inprocessing-proofs'
assert not c.exists()
(c/'logs').mkdir(parents=True)
hashes = {}; records = []; outputs = {}; removed = 0
def bind(p):
    hashes[str(p.relative_to(root))] = sha(p)
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
    cnf.write_text('p cnf 1 1\n1 0\n')
    proof.write_text('2 0 1 0\n' if mode else '0\n')
    checker = root/'tools/checkers/bin'/('cake_lpr' if mode else 'drat-trim')
    p = run('negative-'+str(mode),[checker,cnf,proof])
    assert proof_check_status(checker.name,p.returncode,p.stdout,p.stderr) == 'rejected'
    bind(checker)
    removed += cnf.stat().st_size + proof.stat().st_size
    cnf.unlink(); proof.unlink()
specs = (
    ('sweep','test-sweep-rootfree-proof-6845.cpp',list(product(range(3),range(5),range(2),range(2))),
     '20260910-6872-subsolver-binary-proofs','previous-'),
    ('vivify','test-vivify-count-cache-proof-6386.cpp',list(product(range(6),range(2),range(2),range(2))),
     '20260910-6827-vivify-snapshot-proofs',''),
)
for arm,stamp,stem,source_name in (
    ('previous','20260909','6730-reduce-watermark','reduce-watermark-6730'),
    ('candidate','20260910','6902-watch-rebuild','watch-rebuild-6902'),
):
    authority = root/f'benchmarks/runs/{stamp}-{stem}-build/manifest.json'
    m = json.loads(authority.read_text()); bind(authority)
    tree = root/('.claude/worktrees/runtime-'+source_name)
    for rel,digest in m['files'].items(): assert sha(tree/rel) == digest,rel
    pin = root/('build/pin-'+stem+'-release')
    assert sha(pin/'tactix') == m['binaries'][str((pin/'tactix').relative_to(root))]
    objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
    assert objects
    for p in objects: bind(p)
    outputs[arm] = {}
    for family,fixture_name,cases,old_dir,prefix in specs:
        fixture = root/'benchmarks/probes/scratch'/fixture_name; bind(fixture)
        old_path = root/'benchmarks/runs'/old_dir/'summary.json'; bind(old_path)
        old = json.loads(old_path.read_text())
        assert old['fixture_sha256'] == sha(fixture)
        expected = {r['name']:r for r in old['records']}
        exe = c/f'{arm}-{family}-fixture'
        command = ['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2',
                   '-I'+str(tree/'src'),'-I'+str(tree/'include'),str(fixture),*map(str,objects),'-o',str(exe)]
        p = run(arm+'-'+family+'-build',command)
        assert p.returncode == 0,p.stderr
        bind(exe)
        outputs[arm][family] = []
        for case in cases:
            suffix = '-'.join(map(str,case)); name = f'{arm}-{family}-{suffix}'
            cnf = c/(name+'.cnf'); proof = c/(name+'.proof')
            p = run(name+'-fixture',[exe,*case,cnf,proof])
            assert p.returncode == 0,(name,p.stderr)
            mode = case[-1]
            checker = root/'tools/checkers/bin'/('cake_lpr' if mode else 'drat-trim')
            q = run(name+'-checker',[checker,cnf,proof])
            status = proof_check_status(checker.name,q.returncode,q.stdout,q.stderr)
            assert status == 'verified',(name,status,q.stdout,q.stderr)
            prior = expected[prefix+suffix]
            record = dict(name=name,family=family,arm=arm,checker=checker.name,status=status,
                          input_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size,
                          fixture_stdout=p.stdout,historical_input=prior['input_sha256'],
                          historical_proof=prior['proof_sha256'])
            assert record['input_sha256'] == prior['input_sha256'] and record['proof_sha256'] == prior['proof_sha256'],record
            assert p.stdout == prior['fixture_stdout'],(name,p.stdout,prior['fixture_stdout'])
            records.append(record)
            outputs[arm][family].append((p.stdout,sha(cnf),sha(proof),proof.stat().st_size))
            removed += cnf.stat().st_size + proof.stat().st_size
            cnf.unlink(); proof.unlink()
assert len(records) == 216 and outputs['previous'] == outputs['candidate']
assert sum(r['checker']=='cake_lpr' for r in records) == 108
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
for p in (Path(__file__),root/'tools/bench/proof_oracle.py',root/'tools/bench/test_proof_oracle.py',root/'tools/bench/test_proof_oracle_binaries.py'):
    bind(p)
with (c/'summary.json').open('x') as stream:
    json.dump(dict(checked=216,LRAT_cake_lpr=108,DRAT_drat_trim=108,negative_controls=2,
        hashes=hashes,records=records,removed_bytes=removed,
        scope='Fresh fixed micro-fixture executions of actual6730 and6902;60 Sweep and48 Vivify proofs perarm, exact historical inputs/proofs/stdout and exact cross-arm outputs. Strong independent checkers replace legacy lrat-check-only acceptance for this bounded current-source evidence. Original successful writers/certificates untouched; no real-instance solver run, fixed-reference refresh, fuzz artifact access or promotion.',
        limitation='This closes the selected fixed Sweep/Vivify proof gap, not all historical LRAT checks or current-source fuzz/full50/memory acceptance. Runtime and B/C pending certificates remain independently pending.'),stream,indent=2)
    stream.write('\n')
print('PASS216 strong fixed proofs:108 cake_lpr/108 DRAT;2 negative controls; exact historical and cross-arm certificates; removed',removed,'bytes')
