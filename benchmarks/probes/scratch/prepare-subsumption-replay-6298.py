#!/usr/bin/env python3
"""Native-only successor, byte-replayed against completed6286."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6286-rank50-admission'
authority=root/'benchmarks/runs/20260909-6293-subsumption-producer-build/manifest.json'
b=json.loads(authority.read_text()); source=root/b['source']
for rel,digest in b['files'].items(): assert sha(source/rel)==digest,rel
assert sha(source/'build/kissat')==b['binary_sha256']
c=root/'benchmarks/runs/20260909-6298-rank50-subsumption-producer'
assert not c.exists()
for name in ('pins','traces','results','logs'): (c/name).mkdir(parents=True)
shutil.copy2(source/'build/kissat',c/'pins/kissat')
code=(old/'run.py').read_text().replace('tactix-6286-','tactix-6298-')
anchor="('candidate', 'candidate_control', 'kissat')"
assert code.count(anchor)==1; code=code.replace(anchor,"('kissat',)")
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert code.count(anchor)==1
code=code.replace(anchor,"env['KISSAT_SUBSUMPTION_PRODUCER_TRACE'] = str(c/'traces/kissat.subsumption.tsv')\nenv['SAT_SUBSUMPTION_PRODUCER_LOW'] = '2508'\nenv['SAT_SUBSUMPTION_PRODUCER_HIGH'] = '2510'\n"+anchor)
compile(code,str(c/'run.py'),'exec'); (c/'run.py').write_text(code)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6286-rank50-admission','6298-subsumption-producer').replace('--array=0-2','--array=0-0'))
m=json.loads((old/'manifest.json').read_text())
m.update(observer_build=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),sources=dict(kissat=str((source/'build/kissat').relative_to(root))),
    parent_summary=dict(path=str((old/'summary.json').relative_to(root)),sha256=sha(old/'summary.json')),
    purpose='Identify native source/literal/conflict of subsumption-counter increments2509 and2510.',
    hypothesis='Actual added-literal producer explains new forward material without root/removal growth at823562.',
    validation='Full ordinary trace, admission trace and partial-proof hash identity against6286 native before producer interpretation. No final verdict or runtime claim.',
    test_consideration='Actual native added-literal fixture verifies flips,repeated/opposite polarity,clear/rearm and range filters; schema negative controls. Read-only observer, unchanged12CPU48000MiB/1200solver contract.')
m['hashes']={name:sha(c/name) for name in ('pins/kissat','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
(c/'pins/kissat').chmod((c/'pins/kissat').stat().st_mode & ~0o222)
print(c)
