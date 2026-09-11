#!/usr/bin/env python3
"""Classify full50 outcomes and retain raw work differences before interpretation."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha,reference,score,candidate_rows
from checked_comparison import classify
def module(name,file):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(file));h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h);h.fixtures();return h
def difference(a,b,fp,storage):
    exact=fp.raw_fingerprint(a)==fp.raw_fingerprint(b)
    comparison=storage.compare(a,b)
    al=[s for s in a['solver_statistics'] if not s.startswith('c timing')]
    bl=[s for s in b['solver_statistics'] if not s.startswith('c timing')]
    return dict(classification='EXACT_RAW' if exact else 'STORAGE_ONLY' if comparison['storage_only'] else 'CHANGED_WORK_OR_CERTIFICATE',
        storage=comparison,certificate_delta={k:[a[k],b[k]] for k in ('certificate_sha256','certificate_bytes') if a[k]!=b[k]},
        parent_only=[s for s in al if s not in bl],candidate_only=[s for s in bl if s not in al],
        parent_full=al if not exact else [],candidate_full=bl if not exact else [])
def fixtures(fp,storage):
    lines=['c conflicts   3','c retired     3 vars (1 substituted, 2 eliminated) of 100',
        'c compact     2 passes removed 10 internal variables',
        'c extension   9 vars added, 4 decisions on them (20.00% of 20)',
        'c denseocc    8 slice moves, 0 defrags',
        'c deactivate  10 deactivations, 0 repeated reports, 0 re-bumps of retired vars']
    a=dict(instance='x',verdict='UNSAT',certificate_sha256='a',certificate_bytes=1,solver_statistics=lines)
    assert difference(a,a,fp,storage)['classification']=='EXACT_RAW'
    b=dict(a,solver_statistics=[s.replace('of 100','of 90').replace('removed 10','removed 20') for s in lines])
    assert difference(a,b,fp,storage)['classification']=='STORAGE_ONLY'
    for before,after in [('conflicts   3','conflicts   4'),('10 deactivations','11 deactivations'),('2 passes','3 passes')]:
        r=dict(b,solver_statistics=[s.replace(before,after) for s in b['solver_statistics']])
        d=difference(a,r,fp,storage)
        assert d['classification']=='CHANGED_WORK_OR_CERTIFICATE' and d['parent_full'] and d['candidate_full']
    d=difference(a,dict(b,certificate_sha256='b'),fp,storage)
    assert d['classification']=='CHANGED_WORK_OR_CERTIFICATE' and d['certificate_delta']=={'certificate_sha256':['a','b']}
    print('Full50 classification: exact/storage-only plus four retained changed-work/certificate controls PASS')
def main():
    assert sys.argv[1:] in ([],['--seal'],['--self-test'])
    fp=module('fp','occurrence-fingerprint-7256.py');storage=module('storage','root-storage-statistics-7350.py')
    fixtures(fp,storage)
    if sys.argv[1:]==['--self-test']:return
    c=root/'benchmarks/runs/20260911-7385-corrected-root-ranks1-50'
    target=root/'benchmarks/reports/2026-09-11-corrected-root-full50-7385'
    if '--seal' in sys.argv:assert not target.exists(),'successful full50 snapshot is frozen'
    m=json.loads((c/'manifest.json').read_text());hashes={}
    def bind(p,expected=None):
        d=sha(p);assert expected is None or d==expected,p;hashes[str(p.relative_to(root))]=d
    bind(c/'manifest.json');bind(Path(__file__))
    if (c/'launch.json').exists():bind(c/'launch.json')
    for rel,digest in m['pin_sha256'].items():bind(c/rel,digest)
    for rel,digest in m['evidence_sha256'].items():bind(root/rel,digest)
    ref=reference();bind(root/m['fixed_reference']['path'],m['fixed_reference']['sha256'])
    with (c/'matrix.tsv').open() as stream:matrix=list(csv.DictReader(stream,delimiter='\t'))
    assert matrix==[{k:str(v) for k,v in r.items()} for r in candidate_rows(ref)]
    records={};states={}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r=json.loads(p.read_text());t=r['task'];assert t==int(p.stem)==r['rank']-1 and 0<=t<50
            for key in ('arm','instance','expected'):assert r[key]==matrix[t][key]
            s=classify(r,folder=='results')
            if t in records:assert fp.raw_fingerprint(r)==fp.raw_fingerprint(records[t]) and r['solve']==records[t]['solve']
            if folder=='results':assert r['binary_sha256']==m['arms']['tactix']['sha256']
            bind(p);records[t]=r;states[t]=s
    failures={t:s for t,s in states.items() if s['status'] not in ('pending_validation','checked_SAT','checked_UNSAT')}
    print('Published',len(records),'/50; checked-clean',sum(s['checked'] for s in states.values()))
    assert not failures,('Classify every invalidity or solve loss before scoring',failures)
    parent=root/m['parent_comparison'];bind(parent/'manifest.json');rows=[];deltas={}
    for t,r in sorted(records.items()):
        p=parent/f'results/{t:03d}.json';old=json.loads(p.read_text());assert classify(old,True)['checked'];bind(p)
        d=difference(old,r,fp,storage);deltas[r['rank']]=d;native=ref['records'][t]['measurement']
        seconds,prior,nativetime=(x['solve']['wall_seconds'] for x in (r,old,native))
        rows.append(dict(rank=r['rank'],instance=r['instance'],expected=r['expected'],validation=states[t]['status'],
            parent_work_classification=d['classification'],candidate_seconds=seconds,parent_seconds=prior,kissat_seconds=nativetime,
            candidate_over_parent=seconds/prior,candidate_over_kissat=seconds/nativetime,
            candidate_peak_rss_kib=r['solve']['peak_rss_kb'],parent_peak_rss_kib=old['solve']['peak_rss_kb'],kissat_peak_rss_kib=native['solve']['peak_rss_kb']))
    counts={label:sum(d['classification']==label for d in deltas.values()) for label in ('EXACT_RAW','STORAGE_ONLY','CHANGED_WORK_OR_CERTIFICATE')}
    print('Parent comparisons',counts)
    if len(records)!=50:print('Solver phase incomplete; no full score written');return
    result=score(c);assert result['complete'] and result['expected_solves']==50 and result['reference_checked']==50
    print('Full50',result['candidate_par2'],'seconds;',result['ratio'],'x fixed Kissat')
    print('Changed-work ranks',[r for r,d in deltas.items() if d['classification']=='CHANGED_WORK_OR_CERTIFICATE'])
    if '--seal' in sys.argv:
        target.mkdir()
        with (target/'summary.json').open('x') as out:json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,score=result,
            rows=rows,parent_differences=deltas,parent_classification_counts=counts,
            limitation='Complete candidate-only cross-window view; no isolated unit-cost inference. Every raw difference retained, including proof changes. Pending checks do not count as accepted correctness; final checks, current-source fuzz, memory and consolidation remain separate gates.'),out,indent=2)
        with (target/'comparison.tsv').open('x',newline='') as out:
            w=csv.DictWriter(out,fieldnames=rows[0],delimiter='\t',lineterminator='\n');w.writeheader();w.writerows(rows)
        print(target)
if __name__=='__main__':main()
