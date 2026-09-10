#!/usr/bin/env python3
"""Explain the static address displacement, without claiming its runtime cost."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260909-6797-propagation-placement-origin'
assert not c.exists()
symbols = ('tactix::kernel_rebuild_large_watches(tactix::Kernel&, bool)',
           'tactix::kernel_propagate_beyond_conflicts(tactix::Kernel&)',
           'bool tactix::propagate_literal<false, false, true>(tactix::Kernel&, unsigned int, unsigned int, unsigned long&)',
           'tactix::kernel_propagate(tactix::Kernel&, unsigned int)')
arms = {}
hashes = {str(Path(__file__).relative_to(root)):sha(Path(__file__))}
bodies = {}
for arm, stem in (('previous','6654-congruence-empty-gates'), ('current','6730-reduce-watermark'), ('layout','6763-watermark-layout')):
    p = root/f'benchmarks/runs/20260909-{stem}-build/manifest.json'
    m = json.loads(p.read_text())
    binary = root/f'build/pin-{stem}-release/tactix'
    assert sha(binary) == m['binaries'][str(binary.relative_to(root))]
    source = root/m['source']/'src/kernel/kernel.cpp'
    assert sha(source) == m['files']['src/kernel/kernel.cpp']
    text = source.read_text()
    start = text.index('void kernel_rebuild_large_watches(Kernel &k, bool flush_eliminated) {')
    end = text.index('\nvoid kernel_resume_walk_watches(',start)
    bodies[arm] = text[start:end]
    command = ['nm','-n','-S','-C',str(binary)]
    rows = subprocess.check_output(command,text=True).splitlines()
    selected = {}
    for symbol in symbols:
        found = [r.split(maxsplit=3) for r in rows if r.endswith(' '+symbol)]
        assert len(found) == 1
        row = found[0]
        selected[symbol] = dict(address=int(row[0],16), size=int(row[1],16), type=row[2])
    arms[arm] = dict(command=command, symbols=selected)
    for p in (p,binary,source): hashes[str(p.relative_to(root))] = sha(p)
assert bodies['previous'] == bodies['current'] == bodies['layout']
for arm in ('current','layout'):
    before,after = arms['previous']['symbols'],arms[arm]['symbols']
    assert before[symbols[0]]['address'] == after[symbols[0]]['address']
    assert before[symbols[0]]['size'] - after[symbols[0]]['size'] == 96
    for symbol in symbols[1:]:
        assert before[symbol]['size'] == after[symbol]['size']
        assert before[symbol]['address'] - after[symbol]['address'] == 96
c.mkdir()
out = dict(hashes=hashes, arms=arms, rebuild_source_body_exact=True,
    conclusion='The unchanged-source watch-rebuild body starts at the same address but its generated size falls by96bytes. The next three propagation functions retain sizes and move96bytes earlier in both6730 and6763. This accounts for main propagation0x413790->0x413730.',
    limitation='Static origin of placement only. No proof of compiler bug or runtime cause and no reason to retain extra work or introduce benchmark-tuned padding. All source and binaries remain unchanged.',
    test_consideration='No implementation/parser change: require unique complete demangled symbols, exact independently hash-bound source bodies, exact function sizes and all three address differences.')
with (c/'summary.json').open('x') as stream:
    json.dump(out,stream,indent=2); stream.write('\n')
print(out['conclusion'])
