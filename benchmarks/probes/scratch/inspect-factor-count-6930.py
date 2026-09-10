#!/usr/bin/env python3
"""Bind actual compiled counter dataflow and classify the incomplete harness."""
from pathlib import Path
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6925-factor-count-build'
target = c/'inspection-6930.json'
assert not target.exists()
compile_path = c/'compile-6926.json'
s = json.loads(compile_path.read_text())
hashes = {str(compile_path.relative_to(root)):sha(compile_path),str(Path(__file__).relative_to(root)):sha(Path(__file__))}
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
symbol = '_ZZN6tactixL19factor_apply_seededERNS_6KernelEjENKUljE2_clEj.isra.0'
body = '_ZN6tactixL19factor_apply_seededERNS_6KernelEj'
for arm, expected in (('previous',431),('candidate',419)):
    obj = root/s['objects'][arm]['path']
    assert sha(obj)==s['objects'][arm]['sha256']
    assert s['symbols'][arm][symbol] == expected
    command = ['objdump','-d','--no-show-raw-insn','--disassemble='+symbol,str(obj)]
    out = subprocess.check_output(command,text=True)
    path = c/(arm+'-count-6930.txt')
    with path.open('x') as stream: stream.write(out)
    hashes[str(path.relative_to(root))] = sha(path)
    hashes[str(obj.relative_to(root))] = sha(obj)
assert s['symbols']['previous'][body] == 15593 and s['symbols']['candidate'][body] == 15561
failed = c/'logs/previous-test_rule_instances-link-6927.json'
f = json.loads(failed.read_text())
assert f['exit_code'] == 1 and 'No such file or directory' in f['stderr']
assert not (c/'control-6927.json').exists()
hashes[str(failed.relative_to(root))] = sha(failed)
out = dict(hashes=hashes,body_bytes=dict(previous=15593,candidate=15561),counter_helper_bytes=dict(previous=431,candidate=419),
    finding='Previous counter helper reloads count after no-growth push and reloads vector pointer/count after growth. Candidate holds the counter address/value through both paths. It also places FACTOR/active rejection before the callee-save prologue. Inspectable actual -O3 object output, not a timing gain or exact whole-program code-shape claim. No objcopy or object mutation.',
    incomplete_control='6927 old270 state matrix and oldFactor suite passed; optional test_rule_instances.cpp does not exist, linkfailed before candidatecompile. This is a harness failure, not solver failure or accepted comparison. Logs/objects retained; corrected6928 runs new actual old/new objects and existingkernel suite.',
    preserved='Source-only local dataflow, separate-vector reference remains valid, touched push still precedes count write. No predicate, discovery/tie order, tick/budget, API, guard, proof or persistentlayout change.270 matrix+fullsuites+strongfixedproofs+B/C+threewavecontrol required; unfuzzed/unpromoted.')
with target.open('x') as stream:
    json.dump(out,stream,indent=2);stream.write('\n')
print(json.dumps({k:v for k,v in out.items() if k!='hashes'},indent=2))
