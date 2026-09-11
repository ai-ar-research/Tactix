#!/usr/bin/env python3
"""Bind GC self samples below Reduce to exact instructions and parent totals."""
from collections import Counter
import json
import lzma
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from perf_callstack import samples
from perf_self_pc import bind_self_pc
target = root/'benchmarks/reports/2026-09-11-reduce-garbage-pcs-7229.json'
assert not target.exists()
hashes,results,inventories = {},{},{}
def bind(p,expected=None):
    digest = sha(p); assert expected is None or digest == expected,p
    hashes[str(p.relative_to(root))] = digest
p = root/'benchmarks/reports/2026-09-11-reduce-garbage-7228.json'
parent = json.loads(p.read_text()); bind(p)
for rel,digest in parent['hashes'].items(): bind(root/rel,digest)
for label in ('candidate-wave0','candidate-wave1','candidate-wave2','native'):
    native = label == 'native'
    binary = root/('benchmarks/runs/20260910-7002-largest-gap-release-profiles/pins/kissat' if native else 'benchmarks/runs/20260910-7152-vivify-compact-profiles/pins/candidate')
    path = root/('benchmarks/runs/20260910-7081-rank35-callstack-cost/kissat.samples.txt.xz' if native else f'benchmarks/runs/20260910-7153-vivify-compact-samples/wave{label[-1]}/candidate.samples.txt.xz')
    bind(binary); bind(path)
    entry = 'flush_watched_clauses_by_literal' if native else 'tactix::kernel_collect_garbage('
    outer = 'kissat_reduce' if native else 'tactix::reduce_apply_refs('
    middle = 'kissat_sparse_collect' if native else 'tactix::kernel_sparse_collect_root('
    selected = lambda s:s.startswith(entry)
    if str(binary) not in inventories:
        symbols = {}
        output = subprocess.check_output(['nm','-S','--defined-only','--demangle',str(binary)],text=True)
        for line in output.splitlines():
            m = re.fullmatch(r'([0-9a-f]+) ([0-9a-f]+) [tT] (.+)',line)
            if not m or not selected(m[3]): continue
            address,size,name = int(m[1],16),int(m[2],16),m[3]
            command = ['objdump','-d','-C','--no-show-raw-insn',f'--start-address={address}',f'--stop-address={address+size}',str(binary)]
            assembly = subprocess.check_output(command,text=True)
            rows = [(int(ins[1],16),ins[2]) for line in assembly.splitlines()
                    if (ins := re.fullmatch(r'\s*([0-9a-f]+):\s+(\S.*)',line))]
            assert rows and rows[0][0] == address
            assert all(address <= pc < address+size for pc,_ in rows)
            symbols[name] = dict(address=address,size=size,command=command,assembly=assembly,rows=rows,
                                 instruction_addresses={pc for pc,_ in rows})
        assert symbols
        inventories[str(binary)] = symbols
    inventory = inventories[str(binary)]
    periods = {name:Counter() for name in inventory}
    total = inclusive = self_period = 0
    with lzma.open(path,'rt') as stream:
        for sample in samples(stream):
            has = lambda prefix:any(f.dso == str(binary) and f.symbol.startswith(prefix) for f in sample.frames)
            if not (has(outer) and has(middle)): continue
            total += sample.period
            if not has(entry): continue
            inclusive += sample.period
            leaf = sample.frames[0]
            if leaf.dso != str(binary): continue
            name = bind_self_pc(leaf.pc,leaf.symbol,inventory,selected)
            if name is not None:
                periods[name][leaf.pc] += sample.period; self_period += sample.period
    expected = parent['arms'][label]
    assert total == expected['total_period'] and inclusive == expected['selected_period']
    assert self_period == expected['branches']['no_phase_frame']['<entry self>']
    tables = {}
    for name,symbol in inventory.items():
        rows = [dict(pc=pc,offset=pc-symbol['address'],instruction=asm,period=periods[name][pc]) for pc,asm in symbol['rows']]
        tables[name] = dict(address=symbol['address'],size=symbol['size'],command=symbol['command'],
            disassembly=symbol['assembly'],self_period=sum(periods[name].values()),instruction_rows=rows,
            hottest=sorted(rows,key=lambda r:-r['period'])[:30])
    results[label] = dict(total_period=total,selected_period=inclusive,self_period=self_period,symbols=tables)
    print(label,'exact self periods',self_period,flush=True)
for p in (Path(__file__),root/'tools/bench/perf_self_pc.py',root/'tools/bench/test_perf_self_pc.py'): bind(p)
with target.open('x') as out:
    json.dump(dict(hashes=hashes,arms=results,
        scope='All selected GC self instruction PCs below sparse collection inside Reduce, conserving parent total/inclusive/self quantities in three retained streams and the narrower native watch-flush function.',
        limitation='Sampled PCs and skid, not exact instruction latency or removable cost. Native function has narrower responsibilities. No new solver run, source mutation, profile expansion or causal speedup claim.'),out,indent=2)
print(target)
