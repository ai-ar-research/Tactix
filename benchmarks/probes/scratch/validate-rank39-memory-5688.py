#!/usr/bin/env python3
"""Validate exact-work ownership data before interpreting provisional memory peaks."""
import hashlib
import json
from pathlib import Path
import sys

keys = '''max_rss_kib current_rss_kib vm_kib vars arena_words arena_capacity watch_words watch_capacity watch_heads_capacity reconstruction_words reconstruction_capacity identity_capacity_bytes search_capacity_bytes input_capacity_bytes touch_capacity_bytes event_capacity_bytes probing_capacity_bytes proof_index_capacity_bytes heuristics_capacity_bytes clause_alias_capacity_bytes binary_liveness_capacity_bytes dense_capacity_bytes dense_usable_bytes dense_allocations policy_reduce_capacity_bytes policy_inprocess_capacity_bytes delayed_capacity_bytes heap_used_kib heap_free_kib mmap_kib'''.split()
def parse(line):
    assert line.endswith('\n')
    fields=line.rstrip('\n').split('\t')
    assert len(fields)==len(keys)+2 and fields[0] and fields[1] in ('begin','end')
    pairs=[s.split('=') for s in fields[2:]]
    assert all(len(p)==2 for p in pairs)
    assert [p[0] for p in pairs]==keys
    values={k:int(v) for k,v in pairs}
    assert all(v>=0 for v in values.values())
    for used,cap in [('arena_words','arena_capacity'),('watch_words','watch_capacity'),('reconstruction_words','reconstruction_capacity')]:
        assert values[used]<=values[cap]
    return dict(name=fields[0],stage=fields[1],**values)
def fixtures():
    good='Sweep\tbegin\t'+'\t'.join(k+'=1' for k in keys)+'\n'
    assert parse(good)['name']=='Sweep'
    assert parse(good.replace('begin','end'))['stage']=='end'
    bad=[good.rstrip('\n'),good.replace('Sweep\t','\t'),good.replace('begin','other'),
         good.replace('vars=1','vars=-1'),good.replace('vars=1','vars=nan'),
         good.replace('arena_words=1','arena_words=2'),good.replace('watch_words=1','watch_words=2'),
         good.replace('vars=1','vm_kib=1'),good.replace('\tvars=1','')]
    for line in bad:
        try: parse(line)
        except (AssertionError,ValueError): pass
        else: raise AssertionError('Malformed ownership row accepted')
    print('PASS:two positive/nine negative ownership-row fixtures',flush=True)
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8<<20),b''):h.update(block)
    return h.hexdigest()
def main():
    fixtures()
    if '--self-test' in sys.argv:return
    root=Path(__file__).resolve().parents[3]
    c=root/'benchmarks/runs/20260908-5687-rank39-memory-ownership'
    final=c/'results/000.json';early=c/'provisional/000.json'
    if not final.exists() and not early.exists():print('5687 solver pending');return
    p=final if final.exists() else early
    r=json.loads(p.read_text());m=json.loads((c/'manifest.json').read_text())
    for name,digest in m['pin_sha256'].items():assert sha(c/name)==digest
    assert r['expected_match'] and r['verdict']=='UNSAT' and r['solve']['exit_code']==20 and not r['solve']['timed_out']
    assert r['binary_sha256']==m['arms']['candidate']['sha256']
    old=m['release_record'];assert sha(root/old['record'])==old['record_sha256']
    old=json.loads((root/old['record']).read_text())
    stats=lambda row:[s for s in row['solver_statistics'] if not s.startswith('c timing')]
    assert stats(r)==stats(old),[(a,b) for a,b in __import__('itertools').zip_longest(stats(r),stats(old)) if a!=b]
    assert r['certificate_sha256']==old['certificate_sha256']
    trace=c/'logs/slot-memory.tsv'
    assert sha(trace)==r['solve']['memory_trace']['sha256'] and trace.stat().st_size==r['solve']['memory_trace']['bytes']
    rows=[dict(row=i+1,**parse(line)) for i,line in enumerate(trace.open())]
    assert rows
    fields=['current_rss_kib','max_rss_kib','arena_capacity','watch_capacity','reconstruction_capacity',
            *[k for k in keys if k.endswith('_capacity_bytes')],'heap_used_kib','heap_free_kib','mmap_kib']
    peaks={key:max(rows,key=lambda row:row[key]) for key in fields}
    result=dict(exact_release_work_and_proof=True,parser_fixtures=11,record=str(p.relative_to(root)),record_sha256=sha(p),
        trace_sha256=sha(trace),rows=len(rows),checked_clean=final.exists(),peaks=peaks,
        release_peak_rss_kib=old['solve']['peak_rss_kb'],observed_peak_rss_kib=r['solve']['peak_rss_kb'],
        limitations=m['limitations'])
    target=c/('summary.json' if final.exists() else 'solver-checkpoint.json')
    if final.exists():
        assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
        assert r['proof_check']['exit_code']==0 and not r['proof_check']['timed_out']
    if target.exists():assert json.loads(target.read_text())==result
    else:target.write_text(json.dumps(result,indent=2)+'\n')
    print('5687 exact work/proof;checked',final.exists(),'rows',len(rows))
    print(json.dumps(peaks['current_rss_kib'],indent=2))
if __name__=='__main__':main()
