#!/usr/bin/env python3
"""Fixed 8-position, 4-value blocker-cover controls; no target/solver run."""
import copy,hashlib,itertools,json,lzma,subprocess,tempfile
from pathlib import Path
from check_original_rup import check,Bound
HERE=Path(__file__).resolve().parent
domains=[list(range(4*i+1,4*i+5)) for i in range(24)]
base=list(domains)
for d in domains:base.extend([-a,-b] for a,b in itertools.combinations(d,2))
for i in range(32):base.extend([[-(i+1),97+i],[-(65+i),97+i],[-(97+i),i+1,65+i]])
base.append(list(range(129,161)))
for i in range(32):base.extend([[-(129+i),33+i],[-(129+i),-(97+i)]])

def run(clauses=base, *, query=10000000, seconds=14, malformed=None, sha=None, memory=268435456, bytecap=None):
    raw=(malformed if malformed is not None else 'c fixed\np cnf 160 '+str(len(clauses))+'\n'+''.join(' '.join(map(str,c))+' 0\n' for c in clauses)).encode()
    packed=lzma.compress(raw)
    with tempfile.TemporaryDirectory(prefix='blocker-fixed-') as tmp:
        p=Path(tmp);(p/'input.xz').write_bytes(packed)
        command=[str(HERE/'census' if HERE.name=='pins' else HERE/'pins/census'),str(p/'input.xz'),str(p/'census.json'),sha or hashlib.sha256(packed).hexdigest(),'160',str(len(clauses)),str(bytecap if bytecap is not None else len(raw)),'97',str(seconds),str(query),str(memory),str(len(packed)),'129','0']
        proc=subprocess.run(command,capture_output=True,text=True)
        result=json.loads((p/'census.json').read_text())
        proof=None
        if result['query_complete']:
            proof=check(p/'input.xz',result,hashlib.sha256(packed).hexdigest(),160,len(clauses),len(raw))
            mutated=copy.deepcopy(result);mutated['aligned_tuples'][0]['original_premises']=[]
            try:check(p/'input.xz',mutated,hashlib.sha256(packed).hexdigest(),160,len(clauses),len(raw))
            except AssertionError:pass
            else:raise AssertionError('missing RUP premise accepted')
            mutated=copy.deepcopy(result);mutated['blocker_cover']['blockers'][0]['key_implication']=1
            try:check(p/'input.xz',mutated,hashlib.sha256(packed).hexdigest(),160,len(clauses),len(raw))
            except AssertionError:pass
            else:raise AssertionError('wrong original occurrence accepted')
            try:check(p/'input.xz',result,hashlib.sha256(packed).hexdigest(),160,len(clauses),len(raw),work_cap=1)
            except Bound:pass
            else:raise AssertionError('checker work bound ignored')
        return proc.returncode,result,proof

rc,r,proof=run();assert rc==0 and r['query_complete'] and proof['novel_checked_tuples']==4,(r,proof)
assert len(r['selected_domains'])==16 and len(r['blocker_cover']['blockers'])==32
assert all(t['residual_unexcluded_supports']==[3,3] for t in r['aligned_tuples'])

# Every off-diagonal residual pair has an explicit satisfying original model.
# The third authorized row copies A, so lock ORs admit precisely A's values.
def model(a,u):
    values={}
    for first,code in [(1,a),(33,u),(65,a)]:
        for p,d in enumerate(code):
            for value in range(4):values[first+4*p+value]=value==d
    for i in range(32):values[97+i]=values[i+1] or values[65+i]
    for i in range(32):values[129+i]=values[33+i] and not values[97+i]
    return values
def satisfies(values):return all(any(values[abs(l)]==(l>0) for l in c) for c in base)
for ad,ud in itertools.product(range(4),repeat=2):
    a=[0]*7+[ad];u=[0]*7+[ud];values=model(a,u)
    assert satisfies(values)==(ad!=ud)
    for t in r['aligned_tuples']:
        if satisfies(values):assert any(values[abs(l)]==(l>0) for l in t['clause'])
multi=model([0]*8,[1,1]+[0]*6);assert satisfies(multi) and sum(multi[b] for b in range(129,161))==2
# No blocker AND converse or cover AMO exists in the fixed source.
assert not any(len(c)==3 and c[0]>=129 for c in base)
assert not any(len(c)==2 and all(l<=-129 for l in c) for c in base)
for missing in [list(range(129,161)),[-129,33],[-129,-97],[-97,1,65],[-65,97],[-33,-34],[-1,-2]]:
    rc,r,_=run([c for c in base if c!=missing]);assert not r['query_complete'],(missing,r)
duplicate=base+[[-64,-57,-53,-49,-45,-41,-37,-33,-32,-25,-21,-17,-13,-9,-5,-1]]
_,r,p=run(duplicate);assert sum(bool(t['existing_occurrence']) for t in r['aligned_tuples'])==1 and p['novel_checked_tuples']==3
_,r,p=run(base+[duplicate[-1]+[-64]]);assert p['novel_checked_tuples']==3
_,r,p=run(base+[[-64,-32]]);assert sum(t['binary_implied'] for t in r['aligned_tuples'])==1 and p['novel_checked_tuples']==3
_,r,p=run(base+[[-64,96],[-96,-32]]);assert sum(t['binary_implied'] for t in r['aligned_tuples'])==1 and p['novel_checked_tuples']==3
_,r,p=run(base[:24]+[[-1,1],[-1,-2]]+base[24:]);assert p['novel_checked_tuples']==4
assert run(query=1)[1]['status']=='query_bound'
assert run(seconds=1e-9)[1]['status']=='wall_bound'
assert run(bytecap=1)[1]['status']=='decoded_byte_bound'
assert run(sha='0'*64)[0]==2
assert run(memory=1)[0]==2
raw='p cnf 160 '+str(len(base))+'\n'+''.join(' '.join(map(str,c))+' 0\n' for c in base)
assert run(malformed=raw.rsplit('0',1)[0])[0]==2
assert run(malformed=raw+'1 0\n')[0]==2
print('PASS: four independently original-CNF RUP checked tuples;12 off-diagonal models; no AND converse/cover AMO; missing implication/OR/domain/cover rejection; original/binary/transitive duplicate suppression; original occurrence IDs; checker witness tampering/work; EOF/hash/byte/query/wall/memory bounds')
