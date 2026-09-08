#!/usr/bin/env python3
"""Actual native heap/observer controls; no real-instance solve."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
source = root/'tools/solvers/variants/kissat-bve-heap-observer-5944'
out = root/'benchmarks/runs/20260908-5944-bve-heap-observer-controls/repaired'
out.mkdir(exist_ok=False)
exe = out/'test-observer'
command = ['gcc','-DNDEBUG','-O2','-I'+str(source/'src'),
           str(Path(__file__).with_suffix('.c')),str(source/'build/libkissat.a'),'-lm','-o',str(exe)]
p = subprocess.run(command,capture_output=True,text=True)
assert p.returncode == 0,(p.stdout,p.stderr)
cases = []
for i, subjects in enumerate(('1,2','','0','1,1','1,','1;x','99','1,2,3,4,1,2,3,4,1','missed-initial')):
    trace = out/f'{i}.tsv'
    r = subprocess.run([exe,str(trace),subjects],capture_output=True,text=True)
    assert r.returncode == (0 if i == 0 else 2),(subjects,r.returncode,r.stdout,r.stderr)
    if i == 0:
        lines = trace.read_text().splitlines()
        assert lines[0] == '# bve-heap-observer\t1'
        rows = [dict(x.split('=',1) for x in line.split('\t')[1:]) for line in lines[1:]]
        assert len(rows) == 6 and [x['stage'] for x in rows] == ['initial']*2+['pop']*2+['initial']*2
        assert [x['subject'] for x in rows] == ['1','2']*3
        assert rows[0]['pos']=='0' and rows[2]['pos']=='-1'
        assert rows[4]['pos']!='-1' and float.fromhex(rows[4]['score'])==2.0
    cases.append(dict(subjects=subjects,returncode=r.returncode,stderr=r.stderr))
result = dict(cases=cases,build_command=command,scope='Actual native heap updates, conflict filtering, absence, external remapping and eight rejection controls. No solver invocation.',
              fixture_repair='Initial control crashed before the observer: ASan located null heap->pos in kissat_enlarge_heap, called by kissat_update_heap. Match eliminate.c schedule_variables by reserving schedule capacity before scored imports. Observer source unchanged; original control binaries retained.',
              hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [exe,source/'build/kissat',source/'src/eventtrace.c',source/'src/eliminate.c']})
(out/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
print('PASS: actual native heap observer and eight rejection controls')
