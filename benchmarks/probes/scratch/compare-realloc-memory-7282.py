#!/usr/bin/env python3
"""Separate structural capacity trajectories from observer-sensitive RSS."""
import importlib.util
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
path=Path(__file__).with_name('validate-occurrence-memory-7266.py')
spec=importlib.util.spec_from_file_location('memory_trace',path)
h=importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
PHYSICAL={'max_rss_kib','current_rss_kib','vm_kib','heap_used_kib','heap_free_kib','mmap_kib','dense_usable_bytes'}

def compare(a,b):
    assert a and len(a)==len(b)
    for left,right in zip(a,b):
        assert left.keys()==right.keys()
        assert {k:v for k,v in left.items() if k not in PHYSICAL}=={k:v for k,v in right.items() if k not in PHYSICAL},(left,right)
    return len(a)

def fixtures():
    row=dict(row=1,name='Collect',stage='entry',max_rss_kib=1,arena_capacity=0)
    assert compare([row],[dict(row,max_rss_kib=2)])==1
    for a,b in (([],[]),([row],[]),([row],[dict(row,arena_capacity=1)]),
                ([row],[dict(row,stage='exit')]),([row],[dict(row,new_owner=0)])):
        try: compare(a,b)
        except AssertionError: pass
        else: raise AssertionError('structural mismatch accepted')
    print('Memory comparison: physical-only change and five invalid structural controls PASS')

def main():
    assert sys.argv[1:] in ([],['--self-test'])
    fixtures()
    if sys.argv[1:]: return
    c=root/'benchmarks/runs/20260911-7277-realloc-rank31-memory'
    target=c/'comparison-7282.json'; assert not target.exists(),'successful evidence frozen'
    p=c/'summary.json'; s=json.loads(p.read_text()); hashes={str(p.relative_to(root)):sha(p)}
    for rel,digest in s['hashes'].items():
        assert sha(root/rel)==digest,rel; hashes[rel]=digest
    rows={}
    for task,arm in ((0,'candidate'),(1,'previous')):
        p=c/f'logs/{task:03d}.slot-memory.tsv'
        rows[arm]=h.collect.base.parse_trace(p.read_text())
        hashes[str(p.relative_to(root))]=sha(p)
    count=compare(rows['candidate'],rows['previous'])
    for p in (Path(__file__),path): hashes[str(p.relative_to(root))]=sha(p)
    peaks={r['arm']:r['solver_peak_rss_kib'] for r in s['records']}
    with target.open('x') as out:
        json.dump(dict(hashes=hashes,exact_structural_rows=count,physical_fields=sorted(PHYSICAL),trace_peaks_kib=peaks,
            interpretation='Every traced structural owner capacity, word count, event identity and semantic marker matches. RSS and allocator occupancy may differ. These trace builds do NOT reproduce the repeated Release Rank31 regression: trace candidate505724KiB versus previous509956KiB, while Release is562920 versus495156. Do not assign the Release peak to either traced peak interval.',
            next='A Release-faithful low-overhead ownership observer or allocator-history experiment is needed to localize the Rank31 RSS increase. Preserve the measured regression in every tradeoff and full50 gate.',
            limitation='Exact structural equality between the two diagnostic traces is not equality to unobserved Release capacities, allocator addresses or peak provenance. Original proof checks remain asynchronous.'),out,indent=2)
    print(target,count,'exact structural rows')

if __name__=='__main__': main()
