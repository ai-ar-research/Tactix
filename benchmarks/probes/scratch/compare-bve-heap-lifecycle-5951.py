#!/usr/bin/env python3
"""Require full ordinary replay before interpreting selected heap states."""
from collections import Counter
import hashlib
import io
import itertools
import json
import math
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3]
C=ROOT/'benchmarks/runs/20260908-5949-bve-heap-lifecycle'
SUBJECTS={'225614','225641','225620','89339'}
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8<<20),b''): h.update(b)
    return h.hexdigest()
def parse(stream,arm,subjects=SUBJECTS):
    assert next(stream).strip()=='# bve-heap-observer\t1'
    rows=[]
    for line in stream:
        fields=line.rstrip().split('\t')
        assert fields[0]==('tactix' if arm=='candidate' else 'kissat')
        r=dict(x.split('=',1) for x in fields[1:])
        assert len(r)==len(fields)-1
        assert set(r)=={'stage','trigger','subject','score','positive','negative','pos','root','size'}
        assert r['stage'] in ('initial','pop','update','push','update-add','update-reinsert')
        assert r['subject'] in subjects and int(r['trigger'])>=0 and int(r['root'])>=0
        assert all(int(r[k])>=0 for k in ('positive','negative','size'))
        assert -1<=int(r['pos'])<int(r['size']) or (int(r['pos'])==-1 and int(r['size'])==0)
        assert math.isfinite(float.fromhex(r['score']))
        rows.append(r)
    assert rows
    return rows
def fixtures():
    good='# bve-heap-observer\t1\ntactix\tstage=initial\ttrigger=0\tsubject=225614\tscore=-0x1p+2\tpositive=2\tnegative=3\tpos=0\troot=225614\tsize=4\n'
    assert parse(io.StringIO(good),'candidate')[0]['pos']=='0'
    for old,new in [('stage=initial','stage=unknown'),('subject=225614','subject=0'),
                    ('pos=0','pos=4'),('positive=2','positive=-1'),('score=-0x1p+2','score=nan'),
                    ('trigger=0','root=0'),('tactix','kissat')]:
        try: parse(io.StringIO(good.replace(old,new)),'candidate')
        except (AssertionError,ValueError): pass
        else: raise AssertionError('malformed heap row accepted')
    print('PASS: selected heap parser and seven negative fixtures',flush=True)
def main():
    fixtures()
    if '--self-test' in sys.argv:return
    target=C/'heap-validation.json'
    assert not target.exists()
    if not all((C/'results'/f'{a}.json').exists() for a in ('candidate','kissat')):
        print('Pending terminal results; no evidence output written');return
    m=json.loads((C/'manifest.json').read_text())
    for rel,digest in m['hashes'].items():assert sha(C/rel)==digest,rel
    data,evidence={},{}
    for arm in ('candidate','kissat'):
        p=C/'results'/f'{arm}.json';r=json.loads(p.read_text())
        assert r['exit_code']==0 and r['scratch_deleted']
        assert r['binary_sha256']==sha(C/'pins'/arm)
        binding=m['cached_controls'][arm];oldp=ROOT/binding['record']
        assert sha(oldp)==binding['sha256']
        old=json.loads(oldp.read_text())
        assert r['partial_certificate']==old['partial_certificate']
        for name,digest in old['trace_sha256'].items():assert r['trace_sha256'][name]==digest
        parentp=ROOT/m['parent_campaign']/'results'/f'{arm}.json'
        parent=json.loads(parentp.read_text())
        key=arm+'.bve-candidates.tsv'
        assert r['trace_sha256'][key]==parent['trace_sha256'][key]
        assert set(r['trace_sha256'])-set(old['trace_sha256'])=={key,arm+'.bve-heap.tsv'}
        for name,digest in r['trace_sha256'].items(): assert sha(C/'traces'/name)==digest,name
        with (C/'traces'/f'{arm}.bve-heap.tsv').open() as stream:data[arm]=parse(stream,arm)
        initial=Counter(x['subject'] for x in data[arm] if x['stage']=='initial')
        assert initial=={x:2 for x in SUBJECTS},initial
        evidence[arm]=dict(result_sha256=sha(p),cached_control=binding,parent_record_sha256=sha(parentp),
                           trace_sha256=r['trace_sha256'],rows=len(data[arm]),stages=dict(Counter(x['stage'] for x in data[arm])))
    views={}
    for subject in sorted(SUBJECTS):
        a,b=([r for r in data[arm] if r['subject']==subject] for arm in ('candidate','kissat'))
        fields=('score','positive','negative','pos','root','size')
        first=next((dict(index=i,candidate=x,kissat=y) for i,(x,y) in enumerate(itertools.zip_longest(a,b))
                    if x is None or y is None or any(x[k]!=y[k] for k in fields)),None)
        views[subject]=dict(candidate_rows=len(a),kissat_rows=len(b),first_raw_state_difference=first,
                            initial_candidate=[r for r in a if r['stage']=='initial'],initial_kissat=[r for r in b if r['stage']=='initial'])
    out=dict(full_ordinary_trace_and_partial_proof_replay=True,complete_bve_candidate_replay=True,
             evidence=evidence,subjects=views,
             limitation='Raw selected-state differences still require caller-stage classification: native update is before an absent-member push, Tactix update-reinsert is after. Neither a row-index mismatch nor this diagnostic establishes a runtime gain or checked final proof.')
    target.write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps({k:v for k,v in out.items() if k!='evidence'},indent=2))
if __name__=='__main__':main()
