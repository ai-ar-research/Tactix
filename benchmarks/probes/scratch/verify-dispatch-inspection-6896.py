#!/usr/bin/env python3
"""Recover original compile hashes and classify objcopy's in-place side effect."""
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
shape = json.loads((c/'code-shape-6895.json').read_text())
symbol = '_ZN6tactixL17propagate_literalILb0ELb0ELb1EEEbRNS_6KernelEjjRm'
records = {}; bodies = {}; removed = 0
for arm,original in m['objects'].items():
    old = root/original['path']
    rewritten_sha = sha(old)
    log = json.loads((c/'logs'/(arm+'-compile-6894.json')).read_text())
    fresh = c/(arm+'-recovered-kernel-6896.o'); assert not fresh.exists()
    command = list(log['command']);assert command[-2] == '-o'; command[-1] = str(fresh)
    result = subprocess.run(command,text=True,capture_output=True)
    with (c/'logs'/(arm+'-recover-6896.json')).open('x') as stream:
        json.dump(dict(command=command,exit_code=result.returncode,stdout=result.stdout,stderr=result.stderr),stream,indent=2);stream.write('\n')
    assert result.returncode == 0,result.stderr
    assert sha(fresh) == original['sha256'],'recompiled original bytes must match initial authority'
    rows = subprocess.check_output(['nm','-S','--defined-only',str(fresh)],text=True).splitlines()
    match = [row.split() for row in rows if row.endswith(' '+symbol)]
    assert len(match) == 1
    base,size = (int(v,16) for v in match[0][:2])
    with tempfile.TemporaryDirectory(prefix='safe-section-',dir=c) as directory:
        section = Path(directory)/'text.bin'; copy = Path(directory)/'output.o'
        # An explicit output object keeps objcopy from rewriting its input.
        subprocess.run(['objcopy','--dump-section','.text='+str(section),str(fresh),str(copy)],check=True)
        assert sha(fresh) == original['sha256']
        data = section.read_bytes(); bodies[arm] = data[base:base+size]
        assert len(bodies[arm]) == size
        for region in shape['regions'].values():
            begin,end = int(region['begin'],16),int(region['end'],16)
            assert hashlib.sha256(bodies[arm][begin:end]).hexdigest() == region['sha256'][arm]
        removed += section.stat().st_size + copy.stat().st_size
    records[arm] = dict(original_expected_sha256=original['sha256'],
        rewritten_container=dict(path=original['path'],sha256=rewritten_sha),
        recovered_original=dict(path=str(fresh.relative_to(root)),sha256=sha(fresh)),
        body_sha256=hashlib.sha256(bodies[arm]).hexdigest(),body_bytes=len(bodies[arm]))
assert bodies['previous'] == bodies['candidate']
paths = [Path(__file__),c/'compile-6894.json',c/'code-shape-6895.json']
with (c/'inspection-6896.json').open('x') as stream:
    json.dump(dict(hashes={str(p.relative_to(root)):sha(p) for p in paths},records=records,
        temporary_bytes_removed=removed,
        classification='6895 objcopy --dump-section omitted its output-object argument and rewrote the two helper-owned input object containers. No solver, live pin, source or production artifact was affected. Preserve rewritten containers and original compile record; fresh6896 objects reproduce both original SHA256 values exactly. Explicit separate output now leaves inputs hash-identical; all prior hot-block hashes and entire3865-byte private body agree. Original-path object bindings from6894 are superseded by this classified recovery, never silently restored.',
        conclusion='Failed optimization premise confirmed: byte-identical private propagation. Stop6893 with no solver execution/full build/new runtime gate; no source adoption. No compiler-forcing tuning.'),stream,indent=2);stream.write('\n')
print('PASS recovered exact original object hashes, unchanged safe inputs and byte-identical3865-byte bodies; removed',removed)
