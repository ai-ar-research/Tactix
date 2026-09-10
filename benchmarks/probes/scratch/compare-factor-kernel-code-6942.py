#!/usr/bin/env python3
"""Check linked propagation/reconnect code remains byte/address identical."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6925-factor-count-build'
target = c/'kernel-code-6942.json'; assert not target.exists()
symbols = (
    '_ZN6tactix16kernel_propagateERNS_6KernelEj',
    '_ZN6tactix23kernel_propagate_binaryERNS_6KernelEb',
    '_ZN6tactix24kernel_propagate_probingERNS_6KernelEj',
    '_ZN6tactix28kernel_rebuild_large_watchesERNS_6KernelEb',
    '_ZN6tactix33kernel_propagate_beyond_conflictsERNS_6KernelEb',
)
hashes = {str(Path(__file__).relative_to(root)):sha(Path(__file__))}
records = {}; bodies = {}
for arm, stamp, stem in (('previous','20260909','6730-reduce-watermark'),('candidate','20260910','6925-factor-count')):
    manifest = root/f'benchmarks/runs/{stamp}-{stem}-build/manifest.json'
    m = json.loads(manifest.read_text())
    binary = root/f'build/pin-{stem}-release/tactix'
    assert sha(binary) == m['binaries'][str(binary.relative_to(root))]
    for p in (manifest,binary): hashes[str(p.relative_to(root))] = sha(p)
    sizes = {}
    for line in subprocess.check_output(['nm','-S','--defined-only',str(binary)],text=True).splitlines():
        fields = line.split()
        if len(fields)==4 and fields[-1] in symbols:
            sizes[fields[-1]] = (int(fields[0],16),int(fields[1],16))
    assert set(sizes)==set(symbols)
    records[arm] = {}; bodies[arm] = {}
    for symbol in symbols:
        out = subprocess.check_output(['objdump','-d','--disassemble='+symbol,str(binary)],text=True)
        label = f'<{symbol}>:\n'
        assert out.count(label)==1
        body = out.split(label,1)[1].split('\n\n',1)[0]
        assert body.strip()
        bodies[arm][symbol] = body
        address,size = sizes[symbol]
        records[arm][symbol] = dict(address=hex(address),bytes=size,
            disassembly_sha256=hashlib.sha256(body.encode()).hexdigest())
assert records['previous']==records['candidate'] and bodies['previous']==bodies['candidate']
with target.open('x') as stream:
    json.dump(dict(hashes=hashes,records=records,
        finding='Five linked propagation/reconnect symbols retain exact addresses, sizes, instruction bytes and printed operands. This differs from6902 kernel-source experiment; no unmeasured alignment rescue or code adjustment was used.',
        limitation='These selected linked bodies only, not all CDCL code, runtime parity, instruction-cache equivalence or native work equality. No solver run or object mutation. Existing binary pins remain immutable.'),stream,indent=2)
    stream.write('\n')
print('PASS5 linked propagation/reconnect bodies: exact bytes, addresses and operands')
