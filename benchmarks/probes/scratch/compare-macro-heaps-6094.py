#!/usr/bin/env python3
"""Full parent replay, then bounded-memory comparison of aligned whole heaps."""
from collections import Counter
import hashlib
import importlib.util
import io
import json
import math
from pathlib import Path
import sys

root=Path(__file__).resolve().parents[3]
def module(name,filename):
    sp=importlib.util.spec_from_file_location(name,Path(__file__).with_name(filename))
    m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m);return m
heap=module('heap6094','analyze-rank37-choice-6080.py')
boundary=module('boundary6094','localize-root-lock-rank37-6073.py')
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8<<20),b''):h.update(b)
    return h.hexdigest()

def frame(stream):
    line=stream.readline()
    if not line:return None
    assert line.endswith('\n')
    fields=line.rstrip('\n').split('\t')
    assert len(fields)==5 and fields[0]=='# snapshot' and fields[1]
    conflict,decision,size=map(int,fields[2:])
    assert min(conflict,decision,size)>=0
    return dict(label=fields[1],conflicts=conflict,decisions=decision,size=size)

def load(stream):
    f=frame(stream);assert f is not None
    lines=['# tactix-heap-set\t1','# external\tposition\tscore\tassigned\tactive']
    for _ in range(f['size']):
        line=stream.readline();assert line.endswith('\n')
        lines.append(line.rstrip('\n'))
    assert stream.readline()=='# end\n'
    state=heap.heap(lines)
    for r in state.values():assert math.isfinite(float(r['score'])) and float(r['score'])>=0
    return f,state

def label(raw):
    name,sep,stage=raw.rpartition('_')
    if sep and stage in ('begin','end'):
        return boundary.NAMES.get(name,name)+'_'+stage
    return raw

def index(stream):
    assert stream.readline()=='# macro-heap-series\t1\n'
    frames={};counts=Counter();last=(0,0)
    while True:
        offset=stream.tell();f=frame(stream)
        if f is None:break
        key=(f['conflicts'],f['decisions'],label(f['label']))
        assert key[:2]>=last;last=key[:2]
        occurrence=counts[key];counts[key]+=1
        f['offset']=offset;f['ordinal']=len(frames)
        frames[(*key,occurrence)]=f
        for _ in range(f['size']):
            row=stream.readline();assert row and not row.startswith('#') and row.endswith('\n')
        assert stream.readline()=='# end\n'
    assert frames
    return frames

def fixtures():
    body='# snapshot\tSweep_end\t17\t20\t2\n7\t0\t1.5\t0\t1\n3\t1\t1.5\t1\t0\n# end\n'
    text='# macro-heap-series\t1\n'+body+body
    ix=index(io.StringIO(text));assert len(ix)==2
    assert label('sweep_end')==label('Sweep_end')
    assert label('substitute_early_end')==label('Substitute_end')
    stream=io.StringIO(text);stream.seek(next(iter(ix.values()))['offset'])
    f,h=load(stream);assert f['size']==2 and h[3]['assigned']==1
    bad=[body.replace('snapshot','broken'),body.replace('\t2\n','\t-1\n',1),
         body.replace('1.5','nan'),body.replace('1.5','-1'),body.replace('3\t1','3\t0'),
         body.replace('3\t1','7\t1'),body.replace('\t0\t1\n','\t2\t1\n'),
         body.replace('# end\n',''),body.rstrip('\n')]
    for content in bad:
        try:load(io.StringIO(content))
        except (AssertionError,ValueError):pass
        else:raise AssertionError('malformed frame accepted')
    assert load(io.StringIO('# snapshot\tempty\t17\t20\t0\n# end\n'))[1]=={}
    print('PASS: repeated/empty/indexed frames, explicit name normalization and nine malformed-frame rejects')

def main():
    fixtures()
    if '--self-test' in sys.argv:return
    c=root/'benchmarks/runs/20260908-6092-rank37-macro-heaps'
    target=c/'summary.json';assert not target.exists()
    m=json.loads((c/'manifest.json').read_text());parent=root/m['parent_campaign']
    assert sha(parent/'choice-validation.json')==m['parent_summary_sha256']
    for rel,digest in m['hashes'].items():assert sha(c/rel)==digest,rel
    for rel,digest in m['observer_source_hashes'].items():assert sha(root/rel)==digest,rel
    indices={};hashes={str((c/'manifest.json').relative_to(root)):sha(c/'manifest.json')}
    for arm in ('candidate','kissat'):
        p=c/'results'/f'{arm}.json';r=json.loads(p.read_text())
        old=json.loads((parent/'results'/f'{arm}.json').read_text())
        assert r['exit_code']==0 and r['scratch_deleted']
        assert r['binary_sha256']==m['hashes']['pins/'+arm]
        assert r['partial_certificate']==old['partial_certificate']
        for name,digest in r['trace_sha256'].items():assert sha(c/'traces'/name)==digest,name
        for name,digest in old['trace_sha256'].items():
            assert sha(parent/'traces'/name)==digest==r['trace_sha256'][name],name
        hashes[str(p.relative_to(root))]=sha(p)
        path=c/'traces'/f'{arm}.macro-heaps.tsv'
        hashes[str(path.relative_to(root))]=sha(path)
        with path.open() as stream:indices[arm]=index(stream)
        assert all(k[0]==272979 for k in indices[arm])
    t,k=indices['candidate'],indices['kissat']
    common=sorted(t.keys()&k.keys(),key=lambda key:t[key]['ordinal'])
    assert common and [k[key]['ordinal'] for key in common]==sorted(k[key]['ordinal'] for key in common)
    rows=[];first=None
    with (c/'traces/candidate.macro-heaps.tsv').open() as a,(c/'traces/kissat.macro-heaps.tsv').open() as b:
        for key in common:
            a.seek(t[key]['offset']);b.seek(k[key]['offset'])
            fa,ha=load(a);fb,hb=load(b)
            diff=heap.compare(ha,hb)
            row=dict(key=key,candidate=fa,kissat=fb,counts={n:len(v) for n,v in diff.items()})
            if first is None and any(diff.values()):first=dict(boundary=row,changes=diff,preceding=rows[-1] if rows else None)
            rows.append(row)
    target.write_text(json.dumps(dict(full_parent_replay=True,hashes=hashes,common_boundaries=rows,first_difference=first,
        unmatched={a:[f for key,f in ix.items() if key not in set(common)] for a,ix in indices.items()},
        limitation='Only explicit known names normalized; unmatched sites retained. First differing shared snapshot brackets a transition,not its exact update. Complete heap equality does not imply other formula/assignment/reason state equality.'),indent=2)+'\n')
    print(json.dumps(dict(frame_counts={a:len(v) for a,v in indices.items()},common=len(common),
        first_difference=first['boundary'] if first else None,preceding=first['preceding'] if first else None),indent=2))

if __name__=='__main__':main()
