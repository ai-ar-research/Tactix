#!/usr/bin/env python3
"""Independent original-CNF witness authentication and RUP, never a solver.

Every source occurrence is reconstructed by counting original terminating zeros.
Complete original input ingestion checks final clause count/EOF and absence.
Consistent closed partial assignments certify every unexcluded binary case;
excluded cases carry independently checked original-binary RUP witnesses.
"""
import argparse,hashlib,itertools,json,lzma,resource,time
from pathlib import Path

class Bound(Exception):pass

def check(input_path,census,sha,variables,clause_count,decoded_cap,seconds=29,work_cap=10000000,certificate=None):
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
    support=census['support_analysis'];assert support is not None
    fixedx=support['prefix_unauthorized'];fixedy=support['prefix_authorized']
    assert len(fixedx)==len(fixedy)==7
    for p,(x,y) in enumerate(zip(fixedx,fixedy)):assert x in u[p] and y in a[p] and matched[x]==y
    tuples=census['aligned_tuples'];assert len(tuples)==4
    assert {row['residual_values'][0] for row in tuples}==set(u[-1])
    for row in tuples:
        x,y=row['residual_values'];assert matched[x]==y
        xv=fixedx+[x];yv=[matched[v] for v in xv]
        assert row['clause']==sorted([-v for v in xv+yv])
    cases=support['cases'];assert len(cases)==24 and {c['bit'] for c in cases}==set(range(24))
    cases=sorted(cases,key=lambda c:c['bit'])
    guard=fixedx+fixedy
    valid_bits=0
    for c in cases:
        bit=c['bit']
        extra=[u[-1][bit]] if bit<4 else [a[-1][bit-4]] if bit<8 else [u[-1][(bit-8)//4],a[-1][(bit-8)%4]]
        assert c['assumptions']==guard+extra
        if c['unexcluded']:valid_bits|=1<<bit
    assert certificate is not None
    masks={}
    with Path(certificate).open() as f:
        for line in f:
            charge();assert len(line)<64 and len(masks)<2*variables
            lit,mask=map(int,line.split());assert 0<abs(lit)<=variables and mask>0 and not mask&~valid_bits and lit not in masks
            masks[lit]=mask
    for lit,mask in masks.items():assert not mask&masks.get(-lit,0)
    for c in cases:
        if c['unexcluded']:
            for lit in c['assumptions']:assert masks.get(lit,0)&(1<<c['bit'])
    needed=set(expected)
    for c in cases:needed.update(c['binary_conflict_premises'])
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
                    if len(pending)==2:
                        charge(2);left,right=pending
                        assert not masks.get(-left,0)&~masks.get(right,0),(ordinal,'left closure')
                        assert not masks.get(-right,0)&~masks.get(left,0),(ordinal,'right closure')
                    elif len(pending)==1:
                        assert masks.get(pending[0],0)==valid_bits
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
    for c in cases:
        if not c['unexcluded']:
            assert c['binary_conflict_premises'] and all(len(reconstructed[i])==2 for i in c['binary_conflict_premises'])
            assert rup([-x for x in c['assumptions']],c['binary_conflict_premises'])
    remaining_u=[sum(cases[8+4*i+j]['unexcluded'] and matched[u[-1][i]]!=a[-1][j] for j in range(4)) for i in range(4)]
    remaining_a=[sum(cases[8+4*i+j]['unexcluded'] and matched[u[-1][i]]!=a[-1][j] for i in range(4)) for j in range(4)]
    assert remaining_u==support['remaining_supports_unauthorized'] and remaining_a==support['remaining_supports_authorized']
    nontrivial=sum(cases[i]['unexcluded'] for i in range(4))>=3 and sum(cases[4+i]['unexcluded'] for i in range(4))>=3 and all(not cases[i]['unexcluded'] or remaining_u[i]>=2 for i in range(4)) and all(not cases[4+i]['unexcluded'] or remaining_a[i]>=2 for i in range(4))
    assert nontrivial==support['nontrivial']
    checked=[]
    for row in tuples:
        assert rup(row['clause'],row['original_premises'])
        occurrences=existing[tuple(row['clause'])]
        assert bool(occurrences)==bool(row['existing_occurrence'])
        if occurrences:assert row['existing_occurrence'] in occurrences
        ri=u[-1].index(row['residual_values'][0]);rj=a[-1].index(row['residual_values'][1])
        assert row['binary_implied']==(not cases[8+4*ri+rj]['unexcluded'])
        assert row['residual_unexcluded_supports']==[remaining_u[ri],remaining_a[rj]]
        assert row['novel']==(not occurrences and not row['binary_implied'])
        checked.append(dict(clause=row['clause'],rup=True,existing_occurrences=occurrences,binary_implied=row['binary_implied'],novel=row['novel']))
    return {'schema':'blocker-original-rup-v1','input_hash_verified':True,'original_source_complete':True,'clauses_scanned':ordinal,'decoded_bytes':decoded,'authenticated_original_premises':len(expected),'checked_tuples':checked,'novel_checked_tuples':sum(x['novel'] for x in checked),'rup_work':work,'wall_seconds':time.monotonic()-start,'support_nontrivial':nontrivial,'remaining_supports_unauthorized':remaining_u,'remaining_supports_authorized':remaining_a,'independent_binary_cases':24,'binary_certificate_literals':len(masks),'status':'checked_nontrivial_support_deduction' if nontrivial and any(x['novel'] for x in checked) else 'checked_deduction_support_gate_not_met','binary_exclusion_authority':'Independent complete original-CNF binary scan verifies consistent closed partial assignments for all unexcluded cases; excluded cases independently RUP-check original binary occurrence witnesses','support_interpretation':'At least two supports survive independently checked original-binary closure and aligned forbidden relation under selected prefix. Does not certify satisfiability under other large original clauses.','solver_run':False,'model_validation':'not_applicable_no_solver'}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('input');ap.add_argument('census');ap.add_argument('output');ap.add_argument('sha');ap.add_argument('variables',type=int);ap.add_argument('clauses',type=int);ap.add_argument('decoded',type=int);ap.add_argument('--seconds',type=float,default=29);ap.add_argument('--work',type=int,default=10000000);args=ap.parse_args()
    assert 0<args.seconds<=30 and args.work>0
    resource.setrlimit(resource.RLIMIT_AS,(256*1024*1024,256*1024*1024))
    try:result=check(args.input,json.loads(Path(args.census).read_text()),args.sha,args.variables,args.clauses,args.decoded,args.seconds,args.work,Path(args.census+'.binary-certificate'));rc=0
    except Bound:result={'status':'checker_censored','input_hash_verified':False,'original_source_complete':False,'solver_run':False};rc=3
    except (AssertionError,ValueError,OSError,lzma.LZMAError,MemoryError) as e:result={'status':'checker_rejected','reason':str(e),'solver_run':False};rc=2
    Path(args.output).write_text(json.dumps(result,indent=2)+'\n');return rc
if __name__=='__main__':raise SystemExit(main())
