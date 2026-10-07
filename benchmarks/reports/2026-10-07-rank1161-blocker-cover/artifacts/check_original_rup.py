#!/usr/bin/env python3
"""Independent original-CNF witness authentication and RUP, never a solver.

Every source occurrence is reconstructed by counting original terminating zeros.
Complete original input ingestion checks final clause count/EOF and absence.
The global binary-only exclusion is separately metered by the C++ tool.
"""
import argparse,hashlib,itertools,json,lzma,resource,time
from pathlib import Path

class Bound(Exception):pass

def check(input_path,census,sha,variables,clause_count,decoded_cap,seconds=29,work_cap=10000000):
    start=time.monotonic();work=0
    def charge(n=1):
        nonlocal work
        if time.monotonic()-start>=seconds or n>work_cap-work:raise Bound()
        work+=n
    assert census['source_complete'] and census['query_complete'] and census['input_hash_complete']
    assert census['input_file_sha256']==sha
    expected={}
    def premise(cid,literals):
        assert cid>0
        normalized=tuple(sorted(set(literals)))
        assert cid not in expected or expected[cid]==normalized
        expected[cid]=normalized
    domains={}
    for d in census['selected_domains']:
        values=d['values'];assert len(values)==4 and len(set(values))==4 and all(x>0 for x in values)
        premise(d['alo'],values);domains[d['alo']]=values
        assert len(d['amo'])==6
        for cid,(a,b) in zip(d['amo'],itertools.combinations(values,2)):premise(cid,[-a,-b])
    assert len(domains)==16 and len({x for d in domains.values() for x in d})==64
    definitions={}
    for d in census['authenticated_or_definitions']:
        q=d['output'];assert q>0 and q not in definitions
        definitions[q]=d
        premise(d['reverse'],[-q,*d['inputs']])
        assert len(d['forward'])==len(d['inputs']) and q not in d['inputs']
        for cid,x in zip(d['forward'],d['inputs']):premise(cid,[-x,q])
    cover=census['blocker_cover'];blocks=cover['blockers'];assert len(blocks)==32 and len({b['blocker'] for b in blocks})==32
    premise(cover['alo'],[b['blocker'] for b in blocks])
    for b in blocks:
        premise(b['key_implication'],[-b['blocker'],b['key_literal']])
        premise(b['negative_lock_implication'],[-b['blocker'],-b['lock_literal']])
        assert b['lock_literal'] in definitions
    u=[domains[i] for i in cover['unauthorized_domain_alos']];a=[domains[i] for i in cover['authorized_domain_alos']]
    assert len(u)==len(a)==8
    matched={}
    for ud,ad in zip(u,a):
        position=[b for b in blocks if b['key_literal'] in ud]
        assert len(position)==4 and {b['key_literal'] for b in position}==set(ud)
        for b in position:
            partner=set(definitions[b['lock_literal']]['inputs'])&set(ad)
            assert len(partner)==1;matched[b['key_literal']]=partner.pop()
        assert {matched[x] for x in ud}==set(ad)
    tuples=census['aligned_tuples'];assert len(tuples)==4
    assert {row['residual_values'][0] for row in tuples}==set(u[-1])
    for row in tuples:
        x,y=row['residual_values'];assert matched[x]==y and row['residual_unexcluded_supports']==[3,3]
        xv=[d[0] for d in u[:-1]]+[x];yv=[matched[v] for v in xv]
        assert row['clause']==sorted([-v for v in xv+yv])
    needed=set(expected)
    for row in tuples:needed.update(row['original_premises'])
    raw_hash=hashlib.sha256();compressed=0
    with Path(input_path).open('rb') as f:
        for chunk in iter(lambda:f.read(65536),b''):
            charge();compressed+=len(chunk);assert compressed<=census['compressed_bytes'];raw_hash.update(chunk)
    assert compressed==census['compressed_bytes'] and raw_hash.hexdigest()==sha
    reconstructed={};existing={tuple(row['clause']):[] for row in tuples}
    ordinal=0;pending=[];header=None;decoded=0
    with lzma.open(input_path,'rb') as f:
        for line in f:
            # Streaming ingestion is byte/header bounded independently from
            # the separate finite witness/RUP work counter.
            if time.monotonic()-start>=seconds:raise Bound()
            decoded+=len(line);assert decoded<=decoded_cap
            stripped=line.strip()
            if not stripped or stripped.startswith(b'c'):continue
            if stripped.startswith(b'p'):
                assert header is None and not pending
                words=stripped.split();assert words==[b'p',b'cnf',str(variables).encode(),str(clause_count).encode()]
                header=words;continue
            assert header is not None
            for token in stripped.split():
                value=int(token)
                if value:
                    assert abs(value)<=variables and len(pending)<variables
                    pending.append(value)
                else:
                    ordinal+=1;assert ordinal<=clause_count
                    if ordinal in needed or len(pending)>=16:
                        normalized=tuple(sorted(set(pending)))
                        if ordinal in needed:reconstructed[ordinal]=normalized
                        if normalized in existing:existing[normalized].append(ordinal)
                    pending=[]
    assert header is not None and not pending and ordinal==clause_count and decoded==census['decoded_bytes']
    assert needed==set(reconstructed)
    for cid,literals in expected.items():assert reconstructed[cid]==literals,(cid,literals,reconstructed[cid])
    def rup(candidate,ids):
        values={abs(x):x<0 for x in candidate}
        premises=[reconstructed[cid] for cid in ids]
        while True:
            changed=False
            for c in premises:
                charge(1+len(c))
                if any(abs(x) in values and values[abs(x)]==(x>0) for x in c):continue
                unknown=[x for x in c if abs(x) not in values]
                if not unknown:return True
                if len(unknown)==1:
                    x=unknown[0];values[abs(x)]=x>0;changed=True
            if not changed:return False
    checked=[]
    for row in tuples:
        assert rup(row['clause'],row['original_premises'])
        occurrences=existing[tuple(row['clause'])]
        assert bool(occurrences)==bool(row['existing_occurrence'])
        if occurrences:assert row['existing_occurrence'] in occurrences
        assert row['novel']==(not occurrences and not row['binary_implied'])
        checked.append(dict(clause=row['clause'],rup=True,existing_occurrences=occurrences,binary_implied=row['binary_implied'],novel=row['novel']))
    return {'schema':'blocker-original-rup-v1','input_hash_verified':True,'original_source_complete':True,'clauses_scanned':ordinal,'decoded_bytes':decoded,'authenticated_original_premises':len(expected),'checked_tuples':checked,'novel_checked_tuples':sum(x['novel'] for x in checked),'rup_work':work,'wall_seconds':time.monotonic()-start,'status':'checked_useful_deduction' if any(x['novel'] for x in checked) else 'not_admitted_all_already_explained','binary_exclusion_authority':'Bound census C++ global multi-assumption original-binary closure, validated by fixed controls; not independently rebuilt here','support_interpretation':'Three unexcluded supports in this four-diagonal conditional relation; no global SAT/allowed-tuple claim.','solver_run':False,'model_validation':'not_applicable_no_solver'}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('input');ap.add_argument('census');ap.add_argument('output');ap.add_argument('sha');ap.add_argument('variables',type=int);ap.add_argument('clauses',type=int);ap.add_argument('decoded',type=int);ap.add_argument('--seconds',type=float,default=29);ap.add_argument('--work',type=int,default=10000000);args=ap.parse_args()
    assert 0<args.seconds<=30 and args.work>0
    resource.setrlimit(resource.RLIMIT_AS,(256*1024*1024,256*1024*1024))
    try:result=check(args.input,json.loads(Path(args.census).read_text()),args.sha,args.variables,args.clauses,args.decoded,args.seconds,args.work);rc=0
    except Bound:result={'status':'checker_censored','input_hash_verified':False,'original_source_complete':False,'solver_run':False};rc=3
    except (AssertionError,ValueError,OSError,lzma.LZMAError,MemoryError) as e:result={'status':'checker_rejected','reason':str(e),'solver_run':False};rc=2
    Path(args.output).write_text(json.dumps(result,indent=2)+'\n');return rc
if __name__=='__main__':raise SystemExit(main())
