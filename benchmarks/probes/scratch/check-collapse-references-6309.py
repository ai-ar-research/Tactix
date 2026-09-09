#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6309-collapse-reference-fixtures'
assert not c.exists();c.mkdir();records={}
for arm,variant,fixture,compiler,library,source_file in (
    ('native','kissat-schedule-continue-6206','test-native-collapse-retry-6268.c','gcc','libkissat.a','substitute.c'),
    ('cadical','cadical-extdiag','test-cadical-collapse-6309.cpp','g++','libcadical.a','decompose.cpp')):
    source=root/'tools/solvers/variants'/variant
    fixture=root/'benchmarks/probes/scratch'/fixture
    command=[compiler,'-O2','-I'+str(source/'src'),'-I'+str(source/'build'),str(fixture),str(source/'build'/library),'-lm','-o',str(c/arm)]
    with (c/(arm+'.log')).open('x') as out:
        subprocess.run(command,stdout=out,stderr=subprocess.STDOUT,check=True)
        subprocess.run([str(c/arm)],stdout=out,stderr=subprocess.STDOUT,check=True)
    records[arm]=dict(command=command,fixture_sha256=sha(fixture),library_sha256=sha(source/'build'/library),source_sha256=sha(source/'src'/source_file),output=(c/(arm+'.log')).read_text())
(c/'summary.json').write_text(json.dumps(records,indent=2)+'\n');print({a:r['output'] for a,r in records.items()})
