#!/usr/bin/env python3
"""Original-CNF Lucky lifecycle checks on parent and optimized/sanitized fix."""
import itertools
import json
import os
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
authority=root/'benchmarks/runs/20260911-7369-lucky-roots-validation/manifest.json'
build=json.loads(authority.read_text());assert build['status']=='PASS'
source=root/build['source']
for rel,digest in build['files'].items():assert sha(source/rel)==digest
c=root/'benchmarks/runs/20260911-7368-lucky-root-proofs';assert not c.exists();(c/'logs').mkdir(parents=True)
driver=Path(__file__).with_name('lucky-root-proof-7368.cpp')
hashes={};records=[];removed=0
def bind(p):hashes[str(p.relative_to(root))]=sha(p)
def run(name,command):
    command=list(map(str,command));r=subprocess.run(command,cwd=c,env=env,text=True,capture_output=True)
    path=c/'logs'/(name+'.json')
    with path.open('x') as out:json.dump(dict(command=command,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr),out,indent=2)
    bind(path);return r
def consume(path):
    global removed
    assert path.is_relative_to(c) and path.suffix in ('.cnf','.proof','.out')
    removed+=path.stat().st_size;path.unlink()
def formula(roots,sign,unsat):
    rows=[[1,2],[1,-2]]+[[-1,ev] for ev in range(3,roots+2)]
    b=roots+2
    for g,a,x,y in [(b,b+1,b+2,b+3),(-(b+4),b+5,b+6,b+7),(b+11,b+8,b+9,b+10),(-(b+15),b+12,b+13,b+14)]:
        rows += [[g,a,x],[g,a,-x],[g,-a,y],[g,-a,-y]]
    n=b+15
    if unsat:
        def v(p,h):return n+1+p*2+h
        rows += [[v(p,0),v(p,1)] for p in range(3)]
        rows += [[-v(p,h),-v(q,h)] for h in range(2) for p in range(3) for q in range(p+1,3)]
        n+=6
    return n,[[lit*sign for lit in row] for row in rows]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for arm,pin_stem,kind in [('parent','7340-debug-contracts','release'),('candidate','7366-lucky-roots','release'),('sanitized','7366-lucky-roots','sanitizers')]:
    src=root/'.claude/worktrees/runtime-debug-contracts-7339' if arm=='parent' else source
    pin=root/f'build/pin-{pin_stem}-{kind}';objects=sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'));assert objects
    exe=c/(arm+'-driver');flags=['-O3','-DNDEBUG'] if kind=='release' else ['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
    r=run(arm+'-build',['/cm/local/apps/gcc/13.1.0/bin/g++','-std=c++20','-fno-exceptions','-fno-rtti',*flags,'-I'+str(src/'src'),'-I'+str(src/'include'),driver,*objects,'-o',exe]);assert r.returncode==0,(r.stdout,r.stderr)
    for p in [exe,*objects]:bind(p)
    for roots,sign,unsat,eager,mode in itertools.product((1,5,29),(1,-1),(False,True),(False,True),('drat','lrat')):
        n,rows=formula(roots,sign,unsat);name=f'{arm}-{roots}-{sign}-{int(unsat)}-{int(eager)}-{mode}'
        cnf=c/(name+'.cnf');proof=c/(name+'.proof')
        with cnf.open('x') as out:out.write(f'p cnf {n} {len(rows)}\n'+''.join(' '.join(map(str,row))+' 0\n' for row in rows))
        r=run(name+'-solve',[exe,cnf,proof,mode,'eager' if eager else 'deferred','parent' if arm=='parent' else 'candidate',roots])
        assert r.returncode==(20 if unsat else 10),(name,r.stdout,r.stderr)
        assert 'c Lucky reached fixed=' in r.stdout
        if unsat:
            checker=root/'tools/checkers/bin'/('cake_lpr' if mode=='lrat' else 'drat-trim')
            checked=run(name+'-check',[checker,cnf,proof])
            assert proof_check_status(checker.name,checked.returncode,checked.stdout,checked.stderr)=='verified',(name,checked.stdout,checked.stderr)
        else:
            model=c/(name+'.out')
            with model.open('x') as out:out.write(r.stdout)
            checker=root/'benchmarks/runs/20260911-7327-rank25-root-gdb/pins/checkmodel'
            checked=run(name+'-check',[checker,cnf,model]);assert checked.returncode==0 and checked.stdout.startswith('checkmodel: OK'),(name,checked.stdout,checked.stderr)
            consume(model)
        bind(checker);records.append(dict(name=name,arm=arm,unsat=unsat,mode=mode,eager=eager,
            cnf_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size,reach=r.stdout.splitlines()[0]))
        consume(cnf);consume(proof)
    print(arm,'48 original proof/model checks PASS',flush=True)
for mode in ('drat','lrat'):
    cnf=c/('bad-'+mode+'.cnf');proof=c/('bad-'+mode+'.proof')
    with cnf.open('x') as out:out.write('p cnf 1 1\n1 0\n')
    with proof.open('x') as out:out.write('0\n' if mode=='drat' else '2 0 1 0\n')
    checker=root/'tools/checkers/bin'/('cake_lpr' if mode=='lrat' else 'drat-trim')
    r=run('bad-'+mode,[checker,cnf,proof]);assert proof_check_status(checker.name,r.returncode,r.stdout,r.stderr)=='rejected'
    consume(cnf);consume(proof)
assert len(records)==144
for p in (Path(__file__),driver,authority,root/'tools/bench/proof_oracle.py',root/'benchmarks/runs/20260911-7327-rank25-root-gdb/pins/modelcheck.py'):bind(p)
assert not any(c.glob('*.cnf')) and not any(c.glob('*.proof')) and not any(c.glob('*.out'))
with (c/'manifest.json').open('x') as out:json.dump(dict(status='PASS',hashes=hashes,records=records,
    checked_UNSAT=72,checked_SAT=72,negative_controls=2,removed_bytes=removed,
    scope='Both root signs/counts1,5,29; eager/deferred H; original SAT and UNSAT formulas; parent Release, candidate Release and unsuppressed sanitized candidate. New fixed roots must be inactive in candidate before search resumes.'),out,indent=2)
print(c/'manifest.json')
