#!/usr/bin/env python3
"""Own-binary self-PC attribution; never align differing code by instruction index."""
from collections import Counter,defaultdict
import json
import lzma
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from perf_callstack import samples,phase
from perf_self_pc import bind_self_pc
target=root/'benchmarks/reports/2026-09-11-realloc-propagation-cost-7280.json'
assert not target.exists(),'successful evidence frozen'
hashes={}
def read(p):
    hashes[str(p.relative_to(root))]=sha(p)
    return json.loads(p.read_text())
static=read(root/'benchmarks/runs/20260911-7276-realloc-propagation-code/summary.json')
sampled=read(root/'benchmarks/runs/20260911-7279-realloc-rank38-samples/wave0/summary.json')
campaign=root/'benchmarks/runs/20260911-7278-realloc-rank38-profiles'
m=read(campaign/'manifest.json')
for authority in (static,sampled):
    for rel,digest in authority['hashes'].items():
        assert sha(root/rel)==digest,rel
        hashes[rel]=digest
symbol='tactix::kernel_propagate(tactix::Kernel&, unsigned int)'
mangled='_ZN6tactix16kernel_propagateERNS_6KernelEj'
selected=lambda name:name.startswith('tactix::kernel_propagate(')
arms={}
for arm in ('candidate','previous'):
    code=static['records'][arm][mangled]
    start=int(code['address'],16)
    inventory={symbol:dict(address=start,size=code['size'],instruction_addresses={r['pc'] for r in code['rows']})}
    binary=str(campaign/m['arms'][arm]['path'])
    sites={int(pc):name for pc,name in m['call_sites'][arm]['sites'].items()}
    stream=root/sampled['records'][arm]['sample_stream']
    counts=Counter(); periods=Counter(); scopes=defaultdict(Counter)
    total=count=0
    with lzma.open(stream,'rt') as data:
        for sample in samples(data):
            total+=sample.period; count+=1
            leaf=sample.frames[0]
            if leaf.dso!=binary or not selected(leaf.symbol): continue
            assert bind_self_pc(leaf.pc,leaf.symbol,inventory,selected)==symbol
            scope,_=phase(sample,binary,{m['phase_entry']:None},m['phase_caller'],sites)
            assert scope!='unresolved_phase_caller'
            counts[leaf.pc]+=1; periods[leaf.pc]+=sample.period; scopes[scope][leaf.pc]+=sample.period
    assert (total,count)==(sampled['arms'][arm]['total_period'],sampled['arms'][arm]['samples'])
    assert sum(periods.values())==sum(sum(v.values()) for v in scopes.values())>0
    rows=[dict(**row,period=periods[row['pc']],samples=counts[row['pc']]) for row in code['rows']]
    arms[arm]=dict(address=start,size=code['size'],total_period=total,self_period=sum(periods.values()),
        by_phase={name:sum(v.values()) for name,v in scopes.items()},rows=rows,
        top_self=sorted(rows,key=lambda r:r['period'],reverse=True)[:25])
for p in (Path(__file__),root/'tools/bench/perf_self_pc.py',root/'tools/bench/test_perf_self_pc.py'):
    hashes[str(p.relative_to(root))]=sha(p)
with target.open('x') as out:
    json.dump(dict(hashes=hashes,arms=arms,
        self_period_ratio=arms['candidate']['self_period']/arms['previous']['self_period'],
        complete_propagation_ratio=sampled['complete_propagation_ratio'],phase_ratios=sampled['phase_ratios'],
        test_consideration='Existing strict self-PC interval/instruction and callstack conservation tests. Every selected actual leaf PC must belong to the correct7276 pinned instruction inventory; all sampled periods and phase partitions conserve.',
        limitation='One matched diagnostic pair, not exact instruction latency or a repetition median. Different code bodies are not paired by offset/index. Sampled skid and instruction/code-layout differences do not by themselves prove a cache, predictor, or compiler-heuristic cause.'),out,indent=2)
print(target)
print('Propagation self ratio',arms['candidate']['self_period']/arms['previous']['self_period'])
