#!/usr/bin/env python3
"""Require whole parent replay,then isolate first-call heap removal events."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
root=Path(__file__).resolve().parents[3]
def module(name,filename):
    sp=importlib.util.spec_from_file_location(name,Path(__file__).with_name(filename))
    m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m);return m
series=module('series6102','compare-macro-heaps-6094.py')
pattern=re.compile(r'DEACT (\d+) (\d+) elim=([01]) fixed=([01]) inheap=([01])(?: trail=(\d+))?\n')
def parse(lines,native):
    out=[];last=0
    for line in lines:
        match=pattern.fullmatch(line);assert match,line
        conflict,var,elim,fixed,inheap=map(int,match.groups()[:5])
        trail=match.group(6)
        assert var>0 and conflict>=last and (trail is None)==native
        assert trail is None or int(trail)<=0xffffffff
        out.append(dict(conflict=conflict,variable=var,elim=elim,fixed=fixed,inheap=inheap,
                        trail=int(trail) if trail is not None else None))
        last=conflict
    assert out
    return out
def fixtures():
    good='DEACT 17 3 elim=1 fixed=0 inheap=1\n'
    assert parse([good],True)[0]['variable']==3
    assert parse([good.rstrip('\n')+' trail=4294967295\n'],False)[0]['trail']==0xffffffff
    for bad in (good.rstrip('\n'),good.replace('17','-1'),good.replace(' 3 ',' 0 '),good.replace('inheap=1','inheap=2'),good+'extra',good.replace('elim=1','elim=x')):
        try:parse([bad],True)
        except (AssertionError,ValueError):pass
        else:raise AssertionError('malformed DEACT accepted')
    try:parse([good,good.replace('17','16')],True)
    except AssertionError:pass
    else:raise AssertionError('backward counters accepted')
    print('PASS: native/Tactix framing,sentinel,flags,identity,monotone counters andseven malformed DEACT controls')
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8<<20),b''):h.update(b)
    return h.hexdigest()
def main():
    fixtures()
    if '--self-test' in sys.argv:return
    c=root/'benchmarks/runs/20260908-6100-rank37-substitute-deactivation'
    target=c/'summary.json';assert not target.exists()
    m=json.loads((c/'manifest.json').read_text());parent=root/m['parent_campaign']
    assert sha(parent/'summary.json')==m['parent_summary_sha256']
    for rel,digest in m['hashes'].items():assert sha(c/rel)==digest,rel
    states={};events={};hashes={str((c/'manifest.json').relative_to(root)):sha(c/'manifest.json')}
    for arm in ('candidate','kissat'):
        p=c/'results'/f'{arm}.json';r=json.loads(p.read_text())
        old=json.loads((parent/'results'/f'{arm}.json').read_text())
        assert r['exit_code']==0 and r['scratch_deleted']
        assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+arm]
        assert r['partial_certificate']==old['partial_certificate']
        for name,digest in r['trace_sha256'].items():assert sha(c/'traces'/name)==digest,name
        for name,digest in old['trace_sha256'].items():assert sha(parent/'traces'/name)==digest==r['trace_sha256'][name],name
        hashes[str(p.relative_to(root))]=sha(p)
        path=c/'traces'/f'{arm}.macro-heaps.tsv'
        with path.open() as f:
            index=series.index(f);states[arm]={}
            for stage in ('begin','end'):
                key=(272979,76952961,'Substitute_'+stage,0)
                f.seek(index[key]['offset']);_,states[arm][stage]=series.load(f)
                header='# tactix-heap-set\t1\n# external\tposition\tscore\tassigned\tactive\n'
                h=states[arm][stage]
                text=header+''.join(f"{v}\t{r['position']}\t{r['score']}\t{r['assigned']}\t{r['active']}\n" for v,r in sorted(h.items(),key=lambda p:p[1]['position']))
                fixture=c/f'{arm}.substitute-{stage}.tsv';assert not fixture.exists();fixture.write_text(text)
                hashes[str(fixture.relative_to(root))]=sha(fixture)
        path=c/'traces'/f'{arm}.deact.log'
        with path.open() as f:history=parse(f,arm=='kissat')
        hashes[str(path.relative_to(root))]=sha(path)
        removed=set(states[arm]['begin'])-set(states[arm]['end'])
        chosen=[r for r in history if r['conflict']==272979 and r['inheap'] and r['variable'] in removed]
        assert len(chosen)==len(removed)==len({r['variable'] for r in chosen})
        events[arm]=dict(removed=sorted(removed),heap_mutations=chosen,
                         whole_conflict=[r for r in history if r['conflict']==272979],total_rows=len(history))
        fixture=c/f'{arm}.removal-order.txt';assert not fixture.exists()
        fixture.write_text(''.join(str(r['variable'])+'\n' for r in chosen));hashes[str(fixture.relative_to(root))]=sha(fixture)
    assert states['candidate']['begin']==states['kissat']['begin']
    assert events['candidate']['removed']==events['kissat']['removed']
    a,b=(events[arm]['heap_mutations'] for arm in ('candidate','kissat'))
    fields=('variable','elim','fixed','inheap')
    first=next((dict(index=i,candidate=x,kissat=y) for i,(x,y) in enumerate(zip(a,b)) if any(x[k]!=y[k] for k in fields)),None)
    target.write_text(json.dumps(dict(full_parent_replay=True,hashes=hashes,events=events,first_difference=first,
        limitation='Filter exact removed set of first Substitute call from inheap events at same conflict. Nonmutating/other-phase rows retained separately. Replay these orders through actual heap implementation before claiming the operation order explains the entire topology difference.'),indent=2)+'\n')
    print(json.dumps(dict(removed=len(a),first_difference=first,orders={arm:[r['variable'] for r in e['heap_mutations']] for arm,e in events.items()}),indent=2))
if __name__=='__main__':main()
