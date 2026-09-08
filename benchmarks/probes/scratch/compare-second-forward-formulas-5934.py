#!/usr/bin/env python3
"""Validate ordinary replay, then compare all second-forward formula records."""
from collections import Counter
import csv
import hashlib
import importlib.util
import itertools
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
parser_path = Path(__file__).with_name('compare-rank32-formula-5785.py')
spec = importlib.util.spec_from_file_location('formula_parser',parser_path)
parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parser)
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''): h.update(block)
    return h.hexdigest()
def select(rows,stage,ordinal=1):
    assert stage in ('begin','end') and ordinal >= 0
    matches = [r for r in rows if r['name'].lower()=='strengthen' and r['stage']==stage and int(r['conflicts'])==271079]
    assert len(matches)>ordinal,(stage,ordinal,len(matches))
    return matches[ordinal]
def fixtures():
    parser.fixtures()
    rows = [dict(name='Strengthen',stage='end',conflicts='271079',irred=str(n)) for n in (11,22)]
    assert select(rows,'end')['irred']=='22' and select(rows,'end',0)['irred']=='11'
    for stage,ordinal in [('end',2),('end',-1),('begin',0),('END',0)]:
        try: select(rows,stage,ordinal)
        except AssertionError: pass
        else: raise AssertionError('bad boundary selector accepted')
    print('PASS: zero-based second-call selection andfour negative fixtures',flush=True)
def main():
    fixtures()
    if '--self-test' in sys.argv: return
    for number,stage in ((5931,'begin'),(5932,'end')):
        c = root / f'benchmarks/runs/20260908-{number}-second-forward-{stage}-formula'
        target = c / 'summary.json'
        assert not target.exists()
        m = json.loads((c / 'manifest.json').read_text())
        for rel,digest in m['hashes'].items(): assert sha(c / rel)==digest
        data,evidence = {},{}
        for arm in ('candidate','kissat'):
            p = c / 'results' / (arm+'.json')
            r = json.loads(p.read_text())
            binding = m['cached_controls'][arm]
            oldpath = root / binding['record']
            assert sha(oldpath)==binding['sha256']
            old = json.loads(oldpath.read_text())
            assert r['exit_code']==0 and r['scratch_deleted']
            assert r['binary_sha256']==old['binary_sha256']==sha(c / 'pins' / arm)
            assert r['partial_certificate']==old['partial_certificate']
            for name,digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name)==digest
            for name,digest in old['trace_sha256'].items(): assert r['trace_sha256'][name]==digest
            assert set(r['trace_sha256'])-set(old['trace_sha256'])=={arm+'.formula.tsv'}
            with (c / 'traces' / (arm+'.boundaries.tsv')).open() as stream:
                next(stream)
                header=next(stream).removeprefix('# ').rstrip().split('\t')
                boundary=select(list(csv.DictReader(stream,fieldnames=header,delimiter='\t')),stage)
            data[arm] = parser.read_rows(c / 'traces' / (arm+'.formula.tsv'))
            counts = Counter(row[0][0] for row in data[arm])
            assert counts=={0:int(boundary['irred']),1:int(boundary['red'])},(arm,counts,boundary)
            evidence[arm]=dict(result_sha256=sha(p),cached_record=binding,boundary=boundary,trace_sha256=r['trace_sha256'])
        a,b=data['candidate'],data['kissat']
        ac,bc=Counter(r[0] for r in a),Counter(r[0] for r in b)
        same_order=[r[0] for r in a]==[r[0] for r in b]
        def witness(counter,rows):
            keys=set(counter)
            return [dict(copies=counter[row[0]],row=row) for row in rows if row[0] in keys][:30]
        first=next((dict(index=i,candidate=x,kissat=y) for i,(x,y) in enumerate(itertools.zip_longest(a,b)) if x!=y),None)
        out=dict(stage=stage,observer_parent_trace_and_proof_equal=True,evidence=evidence,
            large_clause_multisets_equal=ac==bc,physical_clause_order_equal=same_order,
            candidate_only_copies=sum((ac-bc).values()),kissat_only_copies=sum((bc-ac).values()),
            candidate_only=witness(ac-bc,a),kissat_only=witness(bc-ac,b),first_physical_difference=first,
            pose_differences=sum(x[1]!=y[1] for x,y in zip(a,b)) if same_order else None,
            metadata_differences=sum(x[2]!=y[2] for x,y in zip(a,b)) if same_order else None,
            root_value_differences=sum(x[3]!=y[3] for x,y in zip(a,b)) if same_order else None,
            limitation='Complete large-clause multiplicities,pose,metadata androot values;not binary order oradmission flags. Witness list capped at30;counts cover full multisets. Same verdict/proof prefix is not runtime orfinal proof acceptance.')
        target.write_text(json.dumps(out,indent=2)+'\n')
        print(json.dumps({k:v for k,v in out.items() if k!='evidence'},indent=2),flush=True)
if __name__=='__main__': main()
