#!/usr/bin/env python3
"""Compare the intended hot blocks before authorizing full build/runtime work."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6893-watch-dispatch-build'
m = json.loads((c/'compile-6894.json').read_text())
for rel,digest in m['hashes'].items():assert sha(root/rel) == digest
symbol = '_ZN6tactixL17propagate_literalILb0ELb0ELb1EEEbRNS_6KernelEjjRm'
bodies = {}; removed = 0
for arm,binding in m['objects'].items():
    obj = root/binding['path'];assert sha(obj) == binding['sha256']
    rows = subprocess.check_output(['nm','-S','--defined-only',str(obj)],text=True).splitlines()
    match = [row.split() for row in rows if row.endswith(' '+symbol)]
    assert len(match) == 1
    base,size,_,_ = match[0]; base=int(base,16);size=int(size,16)
    with tempfile.TemporaryDirectory(prefix='section-',dir=c) as directory:
        p = Path(directory)/'text.bin'
        subprocess.run(['objcopy','--dump-section','.text='+str(p),str(obj)],check=True)
        data = p.read_bytes();removed += len(data)
        bodies[arm] = data[base:base+size]
        assert len(bodies[arm]) == size
out = {}
for name,begin,end in (('watch_decode_blocker',0x207,0x238),
                       ('large_arena_garbage',0x238,0x256),
                       ('large_other_literal',0x256,0x280)):
    parts = {a:b[begin:end] for a,b in bodies.items()}
    out[name] = dict(begin=hex(begin),end=hex(end),bytes=end-begin,
        sha256={a:hashlib.sha256(b).hexdigest() for a,b in parts.items()},
        byte_identical=parts['previous'] == parts['candidate'])
result = dict(hashes={str(p.relative_to(root)):sha(p) for p in (Path(__file__),c/'compile-6894.json',
    root/'benchmarks/runs/20260910-6888-vivify-propagation-annotation/location-6889.json')},
    regions=out,body_bytes={a:len(b) for a,b in bodies.items()},
    differing_body_bytes=sum(x!=y for x,y in zip(bodies['previous'],bodies['candidate'])),
    temporary_section_bytes_removed=removed,
    decision=('Stop this compile-only experiment: the compiler emits byte-identical intended hot blocks; source single-blocker spelling does not change the sampled private scan. No runtime candidate, full build, new tests or promotion warranted for this failed premise.'
              if all(r['byte_identical'] for r in out.values()) else
              'Hot code differs; inspect exact instructions before deciding whether focused controls/full build are warranted.'),
    limitation='Object-code basic blocks only. Whole body may differ in relocation-dependent bytes; not a claim of whole-object identity or correctness. Public/main/probing outer function footprints are not performance evidence. Do not force compiler shape with tuning or attributes. No solver run from this source.')
with (c/'code-shape-6895.json').open('x') as stream:
    json.dump(result,stream,indent=2);stream.write('\n')
print(json.dumps(result,indent=2))
