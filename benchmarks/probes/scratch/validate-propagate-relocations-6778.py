#!/usr/bin/env python3
"""Account every static propagation difference without normalizing data accesses."""
import json
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260909-6777-watermark-propagate-code'
target = c/'relocations-6778.json'
assert not target.exists()
p = c/'summary.json'
s = json.loads(p.read_text())
for rel, digest in s['hashes'].items():
    assert sha(root/rel) == digest, rel
def direct_call(text):
    match = re.fullmatch(r'callq?\s+[0-9a-f]+\s+(<.*>)', text)
    return match[1] if match else None
assert direct_call('callq  412870 <target>') == direct_call('call  412810 <target>')
assert direct_call('callq  412810 <other>') != direct_call('callq  412870 <target>')
assert direct_call('mov 0x168(%r15),%r8') is None
assert direct_call('callq *%rax') is None
hashes = {str(p.relative_to(root)):sha(p), str(Path(__file__).relative_to(root)):sha(Path(__file__))}
bins = {a:root/f'build/pin-{stem}-release/tactix' for a,stem in
        (('before', '6654-congruence-empty-gates'), ('layout', '6763-watermark-layout'))}
constant_reads = []
def read_word(arm, address):
    command = ['objdump', '-s', '--start-address='+hex(address), '--stop-address='+hex(address+2), str(bins[arm])]
    stdout = subprocess.check_output(command, text=True)
    data = [m[1] for line in stdout.splitlines()
            if (m := re.fullmatch(r'\s*'+format(address, 'x')+r'\s+([0-9a-f]{4})\s+.*', line))]
    assert len(data) == 1, stdout
    constant_reads.append(dict(arm=arm, address=address, command=command, stdout=stdout, word_bytes=data[0]))
    return data[0]
before, layout = (s['records'][a] for a in ('before', 'layout'))
assert before['size'] == layout['size'] and len(before['instruction_rows']) == len(layout['instruction_rows'])
relocations = []
for i, (a,b) in enumerate(zip(before['instruction_rows'], layout['instruction_rows'])):
    assert a['offset'] == b['offset'], (i,a,b)
    if a == b:
        continue
    left, right = a['instruction'], b['instruction']
    call = direct_call(left)
    if call:
        assert direct_call(right) == call, (i,a,b)
        relocations.append(dict(index=i, offset=a['offset'], kind='direct_call', target=call))
        continue
    pattern = r'movzwl\s+0x([0-9a-f]+)\(%rip\),(%\w+)\s+#\s+([0-9a-f]+)\s+(<.*>)'
    x, y = re.fullmatch(pattern, left), re.fullmatch(pattern, right)
    assert x and y and x[2] == y[2] and x[4] == y[4], (i,a,b)
    values = [read_word(arm, int(m[3],16)) for arm,m in (('before',x), ('layout',y))]
    assert values[0] == values[1], (i,values)
    relocations.append(dict(index=i, offset=a['offset'], kind='readonly_word', target=x[4], word_bytes=values[0]))
assert sum(r['kind'] == 'direct_call' for r in relocations) == 10
assert sum(r['kind'] == 'readonly_word' for r in relocations) == 2
out = dict(equal_instruction_offsets_and_nonrelocated_operands=True, instruction_rows=len(before['instruction_rows']),
    function_size=before['size'], before_address=before['address_hex'], layout_address=layout['address_hex'],
    relocations=relocations, constant_reads=constant_reads, hashes=hashes,
    test_consideration='Four fixed direct-call recognition controls; every differing actual instruction must be one of10 same-symbol direct calls or2 same-symbol/destination read-only word loads with independently extracted equal bytes. All PCs and other operands exact, so no member-offset or instruction-shape difference is hidden.',
    limitation='Static equivalence of this one function after explicitly proved relocations, not whole-program equivalence or proof that code placement causes the runtime gap.6777 retained callq spellings and RIP operands as differences; this successor accounts them without rewriting the earlier snapshot.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('hashes', 'constant_reads')}, indent=2))
