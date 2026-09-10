#!/usr/bin/env python3
"""Freeze the actual compile-only ITE load-hoisting premise, not a speed claim."""
import json
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build'
target = c/'inspection-6971.json'; assert not target.exists()
compiled = c/'compile-6970.json'; m = json.loads(compiled.read_text())
hashes = {str(p.relative_to(root)):sha(p) for p in (Path(__file__),compiled)}
for rel,digest in m['hashes'].items(): assert sha(root/rel) == digest,rel
symbol = '_ZN6tactixL13extract_gatesERKNS_6KernelERSt6vectorINS_8CongGateESaIS4_EEbPS0_'
regions = {}
for arm,lo,hi,count in (('previous',0x10ad0,0x10aef,11),('candidate',0x10b10,0x10b23,7)):
    obj = root/m['objects'][arm]['path']; assert sha(obj) == m['objects'][arm]['sha256']
    output = subprocess.check_output(['objdump','-dr','--no-show-raw-insn','--disassemble='+symbol,str(obj)],text=True)
    raw = c/f'{arm}-extraction-6971.txt'
    with raw.open('x') as out: out.write(output)
    hashes[str(raw.relative_to(root))] = sha(raw)
    hashes[str(obj.relative_to(root))] = sha(obj)
    rows = []
    for line in output.splitlines():
        match = re.match(r'^\s*([0-9a-f]+):\s+([^\s].*)$',line)
        if match and lo <= int(match[1],16) <= hi:
            rows.append(dict(address='0x'+match[1],assembly=match[2]))
    assert len(rows) == count and rows[0]['address'] == hex(lo) and rows[-1]['address'] == hex(hi)
    regions[arm] = rows
result = dict(hashes=hashes,regions=regions,body_bytes={arm:m['symbols'][arm][symbol] for arm in m['symbols']},
    finding='Previous11-instruction inner comparison prefix reloads outer address/condition/rhs every j. Candidate7-instruction prefix keeps outer condition in a register and reads outer rhs from its saved stack slot only after complementary-condition admission. Both outer field loads occur once per i before inner-loop entry. Enclosing extraction grows16014 to16095 bytes. No objcopy or object mutation.',
    decision='Compiled premise holds for a future bounded actualold/new ITE pair-order test matrix. No timing, independent full build, proof or correctness acceptance for6969 yet. Do not compose with pending6950 or promote.',
    invariants='Local conditional list is immutable during pairing. No new index, sort, persistent layout, guard, hub cutoff, gate verification/seen order or tick/budget change intended.',
    test_consideration='Before any real solver run, add fixed repeated-inner-comparison/duplicate/conditional-order/output-growth and256-cap boundary cases; compare actualold/new complete gate vectors and charged ticks, then full suites, strong ITE-dependent proofs and B/C. Existing generic tests are not asserted sufficient. No fuzz access.')
with target.open('x') as out: json.dump(result,out,indent=2);out.write('\n')
print(result['finding'])
