#!/usr/bin/env python3
"""Read-only paired current memory intervals; optional once-only seal."""
import csv
import importlib.util
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
path=Path(__file__).with_name('validate-occurrence-memory-7266.py')
spec=importlib.util.spec_from_file_location('current_memory',path)
h=importlib.util.module_from_spec(spec); spec.loader.exec_module(h)

def main():
    assert sys.argv[1:] in ([],['--self-test'],['--seal'])
    h.fixtures()
    if sys.argv[1:]==['--self-test']: return
    c=root/'benchmarks/runs/20260911-7277-realloc-rank31-memory'
    target=c/'summary.json'
    if '--seal' in sys.argv: assert not target.exists(),'successful seal frozen'
    m=json.loads((c/'manifest.json').read_text()); hashes={}
    def bind(p,expected=None):
        value=sha(p); assert expected is None or value==expected,p
        hashes[str(p.relative_to(root))]=value
    bind(c/'manifest.json'); bind(c/'launch.json')
    for rel,digest in m['pin_sha256'].items(): bind(c/rel,digest)
    for rel,digest in m['evidence_sha256'].items(): bind(root/rel,digest)
    with (c/'matrix.tsv').open() as stream: matrix=list(csv.DictReader(stream,delimiter='\t'))
    assert len(matrix)==2 and [r['arm'] for r in matrix]==['candidate','previous']
    records,states={},{}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r=json.loads(p.read_text()); bind(p); task=r['task']
            assert task==int(p.stem) and task in (0,1) and r['rank']==31
            for key in ('arm','expected','instance'): assert r[key]==matrix[task][key]
            state=classify(r,folder=='results'); assert state['status'] in ('pending_validation','checked_UNSAT'),(p,state)
            if folder=='results': assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
            if task in records:
                assert h.fingerprint.raw_fingerprint(r)==h.fingerprint.raw_fingerprint(records[task])
                assert r['solve']==records[task]['solve']
            oracle=m['release_records'][r['arm']]; q=root/oracle['path']; bind(q,oracle['sha256'])
            assert h.fingerprint.raw_fingerprint(r)==h.fingerprint.raw_fingerprint(json.loads(q.read_text()))
            records[task],states[task]=r,state
    out=[]
    for task,r in sorted(records.items()):
        p=c/f'logs/{task:03d}.slot-memory.tsv'; trace=r['solve']['memory_trace']
        assert trace['path']==str(p.relative_to(c)) and trace['bytes']==p.stat().st_size
        bind(p,trace['sha256'])
        rows=h.collect.base.parse_trace(p.read_text()); groups=h.collect.collections(rows)
        out.append(dict(arm=r['arm'],state=states[task],collections=len(groups),
            solver_peak_rss_kib=r['solve']['peak_rss_kb'],**h.intervals(rows)))
    for p in (Path(__file__),path,Path(h.collect.__file__),Path(h.collect.base.__file__),Path(h.fingerprint.__file__)): bind(p)
    print('Rank31 paired memory:',len(records),'/2 exact whole outcomes;',sum(s['checked'] for s in states.values()),'checked-clean')
    for r in out:
        print(json.dumps({k:r[k] for k in ('arm','solver_peak_rss_kib','first_peak_sample','previous_sample')},indent=2))
    if '--seal' in sys.argv:
        assert len(records)==2
        with target.open('x') as stream:
            json.dump(dict(hashes=hashes,records=out,limitation=m['limitation']),stream,indent=2)
        print(target)

if __name__=='__main__': main()
