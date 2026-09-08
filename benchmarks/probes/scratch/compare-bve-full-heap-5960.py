#!/usr/bin/env python3
"""Verify full replay, complete heap images and both deterministic sift paths."""
import hashlib
import io
import json
import math
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[3]
C=ROOT/'benchmarks/runs/20260908-5958-bve-full-heap'
FIELDS=('pos','external','score','positive','negative','value','retired','retry')
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8<<20),b''):h.update(b)
    return h.hexdigest()
def parse(f,conflict=271079,subject=122629):
    assert next(f).strip()=='# bve-full-heap\t1'
    groups=[];rows=None
    for line in f:
        cells=line.rstrip().split('\t')
        if cells[0]=='# begin':
            assert rows is None and len(cells)==6
            ordinal=int(cells[1]);assert ordinal==len(groups) and ordinal<2
            assert cells[2]==('initial' if ordinal==0 else 'before-pop')
            assert int(cells[3])==conflict and int(cells[4])==(0 if ordinal==0 else subject)
            count=int(cells[5]);assert count>0;rows=[]
        elif cells[0]=='# end':
            assert rows is not None and cells==['# end',str(len(groups))] and len(rows)==count
            assert len({r['external'] for r in rows})==count
            for i,row in enumerate(rows):
                if i:assert float.fromhex(rows[(i-1)//2]['score'])>=float.fromhex(row['score'])
            if groups:assert rows[0]['external']==subject
            groups.append(rows);rows=None
        else:
            assert rows is not None and len(cells)==8
            row=dict(zip(FIELDS,cells));row={k:(v if k=='score' else int(v)) for k,v in row.items()}
            assert row['pos']==len(rows) and row['external']>0 and math.isfinite(float.fromhex(row['score']))
            assert row['positive']>=0 and row['negative']>=0 and row['value'] in (-1,0,1)
            assert row['retired'] in (0,1) and row['retry'] in (0,1)
            rows.append(row)
    assert rows is None and len(groups)==2
    return groups
def sift(rows):
    heap=[dict(r) for r in rows];removed=heap[0];last=heap.pop();path=[];at=0
    if heap:
        while 2*at+1<len(heap):
            left=2*at+1;right=left+1
            child=right if right<len(heap) and float.fromhex(heap[right]['score'])>float.fromhex(heap[left]['score']) else left
            step=dict(at=at,left=heap[left],right=heap[right] if right<len(heap) else None,selected=child)
            if float.fromhex(heap[child]['score'])<=float.fromhex(last['score']):
                step['stop']=True;path.append(step);break
            path.append(step);heap[at]=heap[child];at=child
        heap[at]=last
    return dict(removed=removed,last=last,last_final_pos=at,path=path,root=heap[0]['external'] if heap else 0)
def fixtures():
    row='0\t1\t0x1p+1\t2\t3\t0\t0\t1\n'
    good='# bve-full-heap\t1\n# begin\t0\tinitial\t1\t0\t1\n'+row+'# end\t0\n# begin\t1\tbefore-pop\t1\t1\t1\n'+row+'# end\t1\n'
    assert len(parse(io.StringIO(good),1,1))==2
    for old,new in [('initial','end'),('0x1p+1','nan'),('\t2\t3\t','\t-2\t3\t'),
                    ('0\t1\t0x','1\t1\t0x'),('# end\t1','# end\t0'),('before-pop\t1\t1','before-pop\t1\t2'),
                    ('\t3\t0\t0\t1','\t3\t2\t0\t1')]:
        try:parse(io.StringIO(good.replace(old,new)),1,1)
        except (AssertionError,ValueError):pass
        else:raise AssertionError('malformed heap accepted')
    rows=[dict(pos=i,external=i+1,score=s) for i,s in enumerate(('0x1p+2','0x1p+1','0x1p+1','0x1p+0'))]
    out=sift(rows);assert out['last_final_pos']==1 and out['root']==2
    print('PASS: full heap schema/seven negatives and left-child tie sift control',flush=True)
def main():
    fixtures()
    if '--self-test' in sys.argv:return
    target=C/'summary.json';assert not target.exists()
    m=json.loads((C/'manifest.json').read_text())
    for rel,digest in m['hashes'].items():assert sha(C/rel)==digest,rel
    arms,evidence={},{}
    for arm in ('candidate','kissat'):
        p=C/'results'/f'{arm}.json';r=json.loads(p.read_text())
        assert r['exit_code']==0 and r['scratch_deleted'] and r['binary_sha256']==sha(C/'pins'/arm)
        bound=m['cached_controls'][arm];oldp=ROOT/bound['record'];assert sha(oldp)==bound['sha256']
        old=json.loads(oldp.read_text());assert r['partial_certificate']==old['partial_certificate']
        for name,digest in old['trace_sha256'].items():assert r['trace_sha256'][name]==digest
        parentp=ROOT/m['parent_campaign']/'results'/f'{arm}.json';parent=json.loads(parentp.read_text())
        key=arm+'.bve-candidates.tsv';assert r['trace_sha256'][key]==parent['trace_sha256'][key]
        assert set(r['trace_sha256'])-set(old['trace_sha256'])=={key,arm+'.full-heap.tsv'}
        for name,digest in r['trace_sha256'].items():assert sha(C/'traces'/name)==digest
        with (C/'traces'/f'{arm}.full-heap.tsv').open() as f:arms[arm]=parse(f)
        evidence[arm]=dict(result_sha256=sha(p),traces=r['trace_sha256'],cached_record=bound)
    snapshots=[]
    for ordinal,(a,b) in enumerate(zip(arms['candidate'],arms['kissat'])):
        am={r['external']:r for r in a};bm={r['external']:r for r in b};common=am.keys()&bm.keys()
        differences={k:sum(am[v][k]!=bm[v][k] for v in common) for k in FIELDS if k!='external'}
        witnesses=[dict(external=v,candidate=am[v],kissat=bm[v]) for v in sorted(common) if am[v]!=bm[v]]
        snapshots.append(dict(ordinal=ordinal,candidate_size=len(a),kissat_size=len(b),candidate_only=sorted(am.keys()-bm.keys()),
            kissat_only=sorted(bm.keys()-am.keys()),differences=differences,first_differences=witnesses[:30],
            complete_rows_equal=a==b))
    paths={arm:sift(images[1]) for arm,images in arms.items()}
    assert paths['candidate']['last']['external']==paths['kissat']['last']['external']==225614
    assert paths['candidate']['last_final_pos']==4465 and paths['kissat']['last_final_pos']==4472
    assert paths['candidate']['root']==paths['kissat']['root']==165965
    target.write_text(json.dumps(dict(full_replay=True,evidence=evidence,snapshots=snapshots,sift_paths=paths,
        limitation='Complete pre-pop heaps and replayed actual selected positions. Sift reconstruction explains placement but does not identify the earlier mutation or authorize tie/score changes.'),indent=2)+'\n')
    print(json.dumps({'snapshots':[{k:v for k,v in s.items() if k!='first_differences'} for s in snapshots],
                      'sift_final_positions':{a:p['last_final_pos'] for a,p in paths.items()}},indent=2))
if __name__=='__main__':main()
