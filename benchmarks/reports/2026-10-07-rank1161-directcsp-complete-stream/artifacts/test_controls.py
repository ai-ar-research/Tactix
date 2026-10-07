#!/usr/bin/env python3
"""Fixed synthetic controls only. No target input or solver execution."""
import hashlib,itertools,json,lzma,subprocess,tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
BIN=HERE/'pins/census'
domains=[[1,2],list(range(3,11)),[11,12]]
base=list(domains)
for d in domains:base.extend([-a,-b] for a,b in itertools.combinations(d,2))
for q,ins in [(13,[1,3,11]),(14,[4,5])]:
    base.extend([-x,q] for x in ins);base.append([-q,*ins])
base.extend([[-13,-14],[-1,-3],[-1,-6],[-1,-7],[-11,-8],[-11,-9],[-11,-10]])

def run(clauses=base, *, text=None, corrupt=False, trailing=False, cap=None, query=10000000, seconds=30, sha=None, memory=268435456, expected_count=None):
    raw=(text if text is not None else 'c fixed control\np cnf 14 '+str(len(clauses))+'\n'+''.join(' '.join(map(str,c))+' 0\n' for c in clauses)).encode()
    packed=lzma.compress(raw)
    if corrupt:packed=packed[:-3]
    if trailing:packed+=b'junk'
    with tempfile.TemporaryDirectory(prefix="directcsp-fixed-") as tmp:
        p=Path(tmp);(p/'input.xz').write_bytes(packed)
        command=[str(BIN),str(p/'input.xz'),str(p/'out.json'),sha or hashlib.sha256(packed).hexdigest(),'14',str(expected_count if expected_count is not None else len(clauses)),str(cap if cap is not None else len(raw)),'13',str(seconds),str(query),str(memory),str(len(packed))]
        proc=subprocess.run(command,capture_output=True,text=True)
        result=json.loads((p/'out.json').read_text())
        return proc.returncode,result

rc,result=run();assert rc==0 and result['source_complete'],result
assert result['path_nogoods'],result
assert any(x['clause']==[-11,-1] and x['supports_before']==[3,3] for x in result['path_nogoods']),result

# Independent clause-local RUP verifier, using only the emitted original IDs.
def rup(clause,ids,formula):
    values={abs(x):x<0 for x in clause}
    premises=[formula[i-1] for i in ids]
    while True:
        changed=False
        for c in premises:
            if any(abs(x) in values and values[abs(x)]==(x>0) for x in c):continue
            unknown=[x for x in c if abs(x) not in values]
            if not unknown:return True
            if len(unknown)==1:
                x=unknown[0];values[abs(x)]=x>0;changed=True
        if not changed:return False
for row in result['helpers']+result['path_nogoods']:assert rup(row['clause'],row['original_premises'],base),row
models=0
for bits in range(1<<14):
    value=lambda x:bool(bits&(1<<(abs(x)-1)))==(x>0)
    if all(any(map(value,c)) for c in base):
        models+=1
        for row in result['helpers']+result['path_nogoods']:assert any(map(value,row['clause']))
assert models>0
def has_target(r):return any(x['clause']==[-11,-1] for x in r['path_nogoods'])
assert not has_target(run(base+[[-1,-11]])[1])
assert not has_target(run(base+[[-1,2],[-2,-11]])[1])
assert not has_target(run(base[:-1])[1])
assert not run([c for c in base if c!=[-1,13]])[1]['helpers']
assert not run([c for c in base if c!=[-13,1,3,11]])[1]['helpers']
assert not run([c for c in base if c!=[-1,-2]])[1]['helpers']
assert run(corrupt=True)[0]==2
assert run(trailing=True)[0]==2
assert run(sha='0'*64)[0]==2
assert run(cap=1)[1]['status']=='decoded_byte_bound'
assert run(query=1)[1]['status']=='query_bound'
assert run(seconds=0.000000001)[1]['status']=='wall_bound'
assert run(memory=1)[0]==2
assert run(text='p cnf 14 10000000\n',expected_count=10000000,memory=32*1024*1024)[1]['status']=='allocation_bound'
assert not has_target(run(base+[[-1,-8],[-1,-9]])[1])
for neighbor in ([-13,14],[-13,-4],[-13,4]):
    modified=[c for c in base if c!=[-13,-14]]+[neighbor]
    _,r=run(modified)
    for row in r['helpers']+r['path_nogoods']:assert rup(row['clause'],row['original_premises'],modified),row
raw='p cnf 14 '+str(len(base))+'\n'+''.join(' '.join(map(str,c))+' 0\n' for c in base)
assert run(text=raw.rsplit('0',1)[0])[0]==2
assert run(text=raw+'1 0\n')[0]==2
assert run(text=raw.replace('p cnf 14','p cnf 15'))[0]==2
# Occurrence IDs include duplicate and tautological original clauses.
extra=[[-1,1],[-1,-2]]
shifted=domains+extra+base[len(domains):]
_,r=run(shifted);assert r['source_complete'] and has_target(r)
for row in r['helpers']+r['path_nogoods']:assert rup(row['clause'],row['original_premises'],shifted)
print('PASS: complete parsing/hash, original occurrence witnesses, RUP, exhaustive soundness, nontrivial support, missing premises, duplicate/reachability suppression, EOF/CRC/header/work/wall/memory bounds')
