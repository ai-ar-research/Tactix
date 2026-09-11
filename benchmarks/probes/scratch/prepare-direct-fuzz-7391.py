#!/usr/bin/env python3
"""One current-source campaign, directly owned, with immutable per-leg harnesses."""
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tools/bench'))
from fixed_runtime import sha
C=ROOT/'benchmarks/runs/20260911-7391-direct-root-fuzz'
AUTH=ROOT/'benchmarks/runs/20260911-7373-lucky-reattach-build/manifest.json'
DEBUG=ROOT/'build/pin-7392-direct-root-fuzz-debug'


def read(p): return json.loads(p.read_text())
def write(p,value):
    with p.open('x') as out: json.dump(value,out,indent=2)
def bound():
    m=read(C/'manifest.json')
    for rel,digest in m['hashes'].items(): assert sha(ROOT/rel)==digest,rel
    return m


def prepare():
    assert not C.exists(), 'campaign already prepared'
    a=read(AUTH); assert a['status']=='PASS'
    source=ROOT/a['source']; hashes={}
    def bind(p,expected=None):
        digest=sha(p); assert expected is None or expected==digest,str(p)
        hashes[str(p.relative_to(ROOT))]=digest
    bind(AUTH); bind(Path(__file__))
    for rel,digest in a['files'].items(): bind(source/rel,digest)
    binary=ROOT/'build/pin-7373-lucky-reattach-release/tactix'
    bind(binary,'fd9c28d555fea83c3a4d41eef099c82c189ad8a05f8f891ea26e3419ef266abe')
    for name in ('tools/solvers/bin/kissat','tools/solvers/bin/cadical','tools/checkers/bin/drat-trim','tools/checkers/bin/lrat-check'):
        p=ROOT/name; assert p.is_file() and os.access(p,os.X_OK); bind(p)
    for name in ('run.sh','slurm.sh','selftest.sh','fuzz.py','generators.py','modelcheck.py','shrink.py','stress.py','ddmin'):
        bind(ROOT/'tools/fuzz'/name)
    for folder in ('corpus','stress-scenarios','broken'):
        for p in sorted((ROOT/'tools/fuzz'/folder).rglob('*')):
            if p.is_file() and '__pycache__' not in p.parts: bind(p)
    if (ROOT/'build/debug/tactix').is_file(): bind(ROOT/'build/debug/tactix')
    for name in ('pins','logs','tmp','outcomes','failures'): (C/name).mkdir(parents=True)
    for src,name in ((binary,'release'),(ROOT/'tools/solvers/bin/kissat','kissat')):
        p=C/'pins'/name; shutil.copy2(src,p); p.chmod(p.stat().st_mode & ~0o222); bind(p)
    legs={
        'release-drat':dict(seed=739100000,solver=str(C/'pins/release'),timeout=10,
            require_reached='Lucky,Substitute,BVE',max_nonterminating_frac=0.10,minimum_verdicts=9000),
        'release-lrat':dict(seed=739110000,solver=str(C/'pins/release')+' --lrat',timeout=10,
            require_reached='Lucky,Substitute,BVE',max_nonterminating_frac=0.10,minimum_verdicts=9000),
        'debug-default':dict(seed=739120000,solver=str(DEBUG/'tactix'),timeout=20,
            require_reached='Lucky,Substitute,BVE',max_nonterminating_frac=0.10,minimum_verdicts=9000),
        'debug-random':dict(seed=739130000,solver=str(DEBUG/'tactix')+' --random-policy={seed}',timeout=10,
            max_nonterminating_frac=0.25,minimum_verdicts=7500),
    }
    write(C/'manifest.json',dict(schema='direct-root-fuzz-7391-v1',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        source=str(source.relative_to(ROOT)),source_authority=str(AUTH.relative_to(ROOT)),hashes=hashes,legs=legs,
        cases_per_leg=10000,physical_cores=50,logical_cpus=100,memory_mib=400000,partition='cpu-dense-hwulab-q',
        ownership='Codex directly owns preparation, submissions, artifact interpretation, shrinking and terminal report under user-approved446ae076. No Claude call or denied external-transfer retry.',
        risk='Root folding/identity reconstruction, Lucky root consumption, heuristic reattachment and collection lifetime. Broad DRAT/native-LRAT checks plus Debug proof-shadow/default/random policy. Original-CNF models and independent proof checks mandatory; all observed faults pause dependent work.',
        coverage='Four seeded broad legs. Default and random timeout populations remain unverified and explicitly bounded; require actual useful verdict counts and reported rule reachability. Generic VariableCompaction counter does not prove root folding; direct lifecycle witness coverage remains a separate required supplement before terminal acceptance.',
        direct_witness_authority='7374 checks144 original-CNF proof/model fixtures plus negative oracles;7373 tests lucky_roots/heuristics_reattach/root_fold/root_watermark protect changed surfaces. Add randomized lifecycle stress if broad legs do not reach physical root folding; do not infer that path from verdict counts.',
        validation='Harness selftest before submission; zero crash/model/proof/differential failures, valid observed seed/coverage/worker records, expected terminal exit, preserved failure bundles and cleanup audit. Pending campaigns are not PASS; no source promotion.',
        test_consideration='No solver or oracle logic changed. Existing harness selftests include deliberately bad model/proof wrappers and shrinking; existing snapshot tests cover immutable harness copying. New wrapper rejects changed source/pins/harness and duplicate preparation/build/submission.'))
    print(C,flush=True)


def selftest():
    bound(); target=C/'outcomes/selftest.json'; assert not target.exists()
    env=dict(os.environ,TMPDIR=str(C/'tmp'))
    command=['bash',str(ROOT/'tools/fuzz/selftest.sh')]
    log=C/'logs/selftest.log'; assert not log.exists()
    with log.open('x') as out: r=subprocess.run(command,cwd=ROOT,env=env,stdout=out,stderr=subprocess.STDOUT)
    write(target,dict(command=command,exit_code=r.returncode,log_sha256=sha(log),
        status='PASS' if r.returncode==0 else 'FAIL',finished_at=dt.datetime.now(dt.timezone.utc).isoformat()))
    assert r.returncode==0,log
    print('Harness selftest PASS',flush=True)


def debug():
    m=bound(); assert not DEBUG.exists(), 'do not rebuild an owned pin'
    target=C/'outcomes/debug-build.json'; assert not target.exists()
    source=ROOT/m['source']; tests=['lucky_roots','heuristics_reattach','root_fold','root_watermark','collect_connection']
    commands=[['cmake','-S',str(source),'-B',str(DEBUG),'-DCMAKE_BUILD_TYPE=Debug','-DTACTIX_ENABLE_PROOF_SHADOW=ON'],
        ['cmake','--build',str(DEBUG),'-j8','--target','tactix',*['test_'+t for t in tests]],
        ['ctest','--test-dir',str(DEBUG),'--output-on-failure','-R','^('+'|'.join(tests)+')$']]
    log=C/'logs/debug-build.log'; assert not log.exists()
    with log.open('x') as out:
        for command in commands:
            r=subprocess.run(command,cwd=ROOT,stdout=out,stderr=subprocess.STDOUT)
            if r.returncode:
                write(target,dict(status='FAIL',command=command,exit_code=r.returncode,log_sha256=sha(log)))
                raise SystemExit(r.returncode)
    cache=(DEBUG/'CMakeCache.txt').read_text(); assert 'TACTIX_ENABLE_PROOF_SHADOW:BOOL=ON' in cache
    assert 'CMAKE_BUILD_TYPE:STRING=Debug' in cache
    assert '100% tests passed' in log.read_text() and 'out of 5' in log.read_text()
    write(target,dict(status='PASS',commands=commands,source_authority_sha256=sha(AUTH),log_sha256=sha(log),
        binary_sha256=sha(DEBUG/'tactix'),cache_sha256=sha(DEBUG/'CMakeCache.txt'),tests=tests))
    print('Current Debug/proof-shadow build and five lifecycle fixtures PASS',flush=True)


def submit(leg):
    m=bound(); assert leg in m['legs']
    assert read(C/'outcomes/selftest.json')['status']=='PASS'
    target=C/f'outcomes/launch-{leg}.json'; assert not target.exists(), 'leg already submitted'
    spec=m['legs'][leg]
    if leg.startswith('debug'):
        d=read(C/'outcomes/debug-build.json'); assert d['status']=='PASS' and sha(DEBUG/'tactix')==d['binary_sha256']
    cmd=['bash',str(ROOT/'tools/fuzz/slurm.sh'),'--name','7391-'+leg,'--time','05:00:00','--',
        '--quick','--solver',spec['solver'],'--oracle',str(C/'pins/kissat'),'--seed',str(spec['seed']),
        '--timeout',str(spec['timeout']),'--max-nonterminating-frac',str(spec['max_nonterminating_frac']),
        '--minimum-verdicts',str(spec['minimum_verdicts']),'--max-failures','1','--outdir',str(C/'failures'/leg)]
    if 'require_reached' in spec: cmd += ['--require-reached',spec['require_reached']]
    since=dt.datetime.now().replace(microsecond=0).isoformat()
    r=subprocess.run(cmd,cwd=ROOT,text=True,capture_output=True)
    write(target,dict(command=cmd,accounting_since=since,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr,
        manifest_sha256=sha(C/'manifest.json')))
    print(r.stdout,r.stderr,flush=True); assert r.returncode==0,target


if __name__=='__main__':
    if sys.argv[1:]==['--prepare']: prepare()
    elif sys.argv[1:]==['--selftest']: selftest()
    elif sys.argv[1:]==['--debug']: debug()
    elif len(sys.argv)==3 and sys.argv[1]=='--submit': submit(sys.argv[2])
    else: raise SystemExit('use --prepare, --selftest, --debug or --submit LEG')
