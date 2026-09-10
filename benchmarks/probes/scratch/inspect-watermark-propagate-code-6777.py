#!/usr/bin/env python3
"""Compare immutable propagation code placement and normalized instructions."""
import itertools
import json
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
c = root/'benchmarks/runs/20260909-6777-watermark-propagate-code'
assert not c.exists()
c.mkdir()
symbol = '_ZN6tactix16kernel_propagateERNS_6KernelEj'
hashes = {str(Path(__file__).relative_to(root)):sha(Path(__file__))}
records = {}
for arm, stem in (('before', '6654-congruence-empty-gates'),
                  ('watermark', '6730-reduce-watermark'), ('layout', '6763-watermark-layout')):
    authority = root/f'benchmarks/runs/20260909-{stem}-build/manifest.json'
    m = json.loads(authority.read_text())
    binary = root/f'build/pin-{stem}-release/tactix'
    assert sha(binary) == m['binaries'][str(binary.relative_to(root))]
    for p in (authority, binary):
        hashes[str(p.relative_to(root))] = sha(p)
    found = [row.split() for row in subprocess.check_output(['nm', '-S', str(binary)], text=True).splitlines()
             if row.split()[-1:] == [symbol]]
    assert len(found) == 1 and found[0][2] == 'T'
    address, size = int(found[0][0], 16), int(found[0][1], 16)
    command = ['objdump', '-d', '--no-show-raw-insn', '--disassemble='+symbol, str(binary)]
    raw = subprocess.check_output(command, text=True)
    path = c/(arm+'.asm')
    path.write_text(raw)
    hashes[str(path.relative_to(root))] = sha(path)
    rows = []
    for line in raw.splitlines():
        match = re.match(r'^\s*([0-9a-f]+):\s+(.*)$', line)
        if not match:
            continue
        pc, instruction = int(match[1], 16), match[2].strip()
        assert address <= pc < address+size
        # For direct branches retain the full symbolic target and offset;
        # remove only its redundant relocated absolute address. All other
        # operands (including memory displacements) and relative PCs stay exact.
        canonical = re.sub(r'^(call|j[a-z]+)\s+[0-9a-f]+\s+(<.*>)$', r'\1 \2', instruction)
        rows.append(dict(offset=pc-address, instruction=canonical))
    assert rows and rows[0]['offset'] == 0
    records[arm] = dict(address=address, address_hex=hex(address), size=size,
        address_mod64=address % 64, command=command, instruction_rows=rows)
comparisons = {}
for left, right in (('before', 'layout'), ('watermark', 'layout')):
    differences = [dict(index=i, left=a, right=b) for i,(a,b) in
        enumerate(itertools.zip_longest(records[left]['instruction_rows'], records[right]['instruction_rows'])) if a != b]
    comparisons[left+'_vs_'+right] = dict(equal=not differences, differences=len(differences), first=differences[:20])
out = dict(records=records, comparisons=comparisons, hashes=hashes,
    normalization='Keep every instruction-relative PC and operand. Direct branch/call absolute addresses only are replaced by the unchanged full symbolic target. No RIP/memory/immediate or code-shape normalization.',
    limitation='Static placement/code evidence only, not proof of front-end, cache, branch-predictor or runtime causality.64-byte residues are descriptive placement, not a tuned solver constant or proposed production alignment.')
with (c/'summary.json').open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps(dict(placement={a:{k:v for k,v in r.items() if k not in ('instruction_rows', 'command')} for a,r in records.items()}, comparisons=comparisons), indent=2))
