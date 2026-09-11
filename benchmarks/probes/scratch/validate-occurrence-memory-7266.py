#!/usr/bin/env python3
"""Validate current memory attribution before selecting a storage repair."""
import csv
import importlib.util
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

def module(name,filename):
    path = Path(__file__).with_name(filename)
    spec = importlib.util.spec_from_file_location(name,path)
    value = importlib.util.module_from_spec(spec); spec.loader.exec_module(value)
    return value
collect = module('collection_memory','seal-collect-memory-6153.py')
fingerprint = module('occurrence_fingerprint','occurrence-fingerprint-7256.py')

def intervals(rows):
    assert rows and all(a['max_rss_kib'] <= b['max_rss_kib'] for a,b in zip(rows,rows[1:]))
    capacities = []
    for row in rows:
        if row['name'] != 'BVECapacity': continue
        assert row['stage'] in ('begin','end','postflush')
        assert 0 < row['memory_subject_internal'] <= row['vars']
        assert row['dense_large_arena_capacity_bytes'] + row['dense_ordered_arena_capacity_bytes'] <= row['dense_capacity_bytes']
        capacities.append(row)
    assert capacities, 'no BVE capacity observation'
    peak = max(rows,key=lambda row:row['max_rss_kib'])
    at = rows.index(peak)
    return dict(rows=len(rows),capacity_transitions=len(capacities),
        first_peak_sample=peak,previous_sample=rows[at-1] if at else None,
        peak_increases=[dict(previous=rows[i-1] if i else None,current=row)
            for i,row in enumerate(rows) if not i or row['max_rss_kib'] > rows[i-1]['max_rss_kib']])

def fixtures():
    collect.self_test(); fingerprint.fixtures()
    row = dict(name='BVECapacity',stage='begin',max_rss_kib=10,
        memory_subject_internal=1,vars=2,dense_large_arena_capacity_bytes=0,
        dense_ordered_arena_capacity_bytes=8,dense_capacity_bytes=8)
    rows = [dict(row,row=1),dict(row,row=2,max_rss_kib=20),dict(row,row=3,max_rss_kib=20)]
    value = intervals(rows)
    assert value['first_peak_sample']['row'] == 2 and value['previous_sample']['row'] == 1
    assert len(value['peak_increases']) == 2
    assert intervals([dict(row,dense_capacity_bytes=0,dense_ordered_arena_capacity_bytes=0)])['previous_sample'] is None
    invalid = [[],[dict(row,name='Probe')],rows[::-1],
        [dict(row,stage='unknown')],[dict(row,memory_subject_internal=0)],
        [dict(row,memory_subject_internal=3)],[dict(row,dense_capacity_bytes=4)]]
    for bad in invalid:
        try: intervals(bad)
        except AssertionError: pass
        else: raise AssertionError('invalid capacity interval accepted')
    print('Current occurrence memory: first-peak/tie/previous/zero controls and seven invalid intervals PASS')

def main():
    assert sys.argv[1:] in ([],['--self-test'],['--seal'])
    fixtures()
    if sys.argv[1:] == ['--self-test']: return
    c = root/'benchmarks/runs/20260911-7265-occurrence-memory'
    target = c/'summary-7266.json'
    if '--seal' in sys.argv: assert not target.exists(), 'successful seal frozen'
    hashes = {}
    def bind(p,expected=None):
        digest = sha(p); assert expected is None or digest == expected,p
        hashes[str(p.relative_to(root))] = digest
    p = c/'manifest.json'; m = json.loads(p.read_text()); bind(p)
    bind(c/'launch.json')
    for rel,digest in m['pin_sha256'].items(): bind(c/rel,digest)
    for rel,digest in m['evidence_sha256'].items(): bind(root/rel,digest)
    with (c/'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream,delimiter='\t'))
    assert [int(row['rank']) for row in matrix] == [9,25]
    records,states = {}, {}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r = json.loads(p.read_text()); bind(p); task = r['task']
            assert task == int(p.stem) and task in (0,1)
            row = matrix[task]
            assert r['rank'] == int(row['rank'])
            for key in ('arm','instance','expected'): assert r[key] == row[key]
            state = classify(r,folder == 'results')
            assert state['status'] in ('pending_validation','checked_SAT'),(p,state)
            if folder == 'results': assert r['binary_sha256'] == m['arms']['tactix']['sha256']
            if task in records:
                assert fingerprint.raw_fingerprint(r) == fingerprint.raw_fingerprint(records[task])
                assert r['solve'] == records[task]['solve']
            oracle = m['release_records'][str(r['rank'])]
            old = root/oracle['path']; bind(old,oracle['sha256'])
            release = json.loads(old.read_text())
            assert fingerprint.raw_fingerprint(r) == fingerprint.raw_fingerprint(release),('observer changed work/proof',r['rank'])
            records[task],states[task] = r,state
    out = []
    for task,r in sorted(records.items()):
        trace = c/f'logs/{task:03d}.slot-memory.tsv'
        proof = r['solve']['memory_trace']
        assert proof['path'] == str(trace.relative_to(c)) and trace.stat().st_size == proof['bytes']
        bind(trace,proof['sha256'])
        rows = collect.base.parse_trace(trace.read_text())
        groups = collect.collections(rows)
        info = intervals(rows)
        out.append(dict(rank=r['rank'],state=states[task],collections=len(groups),
            observed_solver_peak_rss_kib=r['solve']['peak_rss_kb'],
            observed_trace_peak_rss_kib=info['first_peak_sample']['max_rss_kib'],
            exact_own_release_work_and_certificate=True,**info))
    for p in (Path(__file__),Path(collect.__file__),Path(collect.base.__file__),Path(fingerprint.__file__)):
        bind(p)
    print('Memory diagnostic:',len(records),'/2 exact whole Release outcomes;',sum(v['checked'] for v in states.values()),'checked-clean')
    for value in out:
        print(json.dumps({key:value[key] for key in ('rank','collections','rows','capacity_transitions','first_peak_sample','previous_sample','observed_solver_peak_rss_kib')},indent=2))
    if '--seal' not in sys.argv: return
    assert len(records) == 2, 'incomplete solver outcomes'
    with target.open('x') as stream:
        json.dump(dict(hashes=hashes,records=out,checked_clean=sum(v['checked'] for v in states.values()),
            limitation=m['limitations'],interpretation='Current capacity intervals only. Peak RSS may precede its first sampled row or occur after the last trace. No unsampled allocation, historical peak cause, Release RSS saving or current-source fuzz acceptance is inferred.'),stream,indent=2)
    print(target)

if __name__ == '__main__': main()
