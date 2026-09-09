#!/usr/bin/env python3
"""Add only sort-key snapshots to the three fully validated Rank45 censuses."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6410-rank45-transitive-census'
c=root/'benchmarks/runs/20260909-6415-rank45-transitive-schedule'; assert not c.exists()
s=json.loads((old/'summary.json').read_text()); assert s['parent_replay']
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest
authorities={a:root/('benchmarks/runs/'+name+'/manifest.json') for a,name in (
    ('candidate','20260909-6413-rank45-schedule-build'),('previous','20260909-6350-transitive-schedule-build'),
    ('kissat','20260909-6350-transitive-schedule-build'))}
builds={a:json.loads(p.read_text()) for a,p in authorities.items()}
for a,b in builds.items():
    assert b['release_exact_parent']
    sources={b['source']:b['files']} if 'source' in b else b['files']
    for source,files in sources.items():
        for rel,digest in files.items(): assert sha(root/source/rel)==digest
    for rel,digest in b['binaries'].items(): assert sha(root/rel)==digest
binaries=dict(candidate='build/pin-6413-rank45-schedule-trace/tactix',
    previous='build/pin-6350-transitive-schedule-trace/tactix',
    kissat='tools/solvers/variants/kissat-transitive-schedule-6350/build/kissat')
for name in ('pins','logs','results','traces'): (c/name).mkdir(parents=True)
for arm,binary in binaries.items(): shutil.copy2(root/binary,c/'pins'/arm)
m=json.loads((old/'manifest.json').read_text())
runner=(old/'run.py').read_text().replace('tactix-6410-','tactix-6415-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"; assert runner.count(anchor)==1
extra="env['SAT_TRANSITIVE_SCHEDULE_PATH'] = str(c / 'traces' / (arm + '.schedule.tsv'))\nenv['SAT_TRANSITIVE_SCHEDULE_CONFLICT'] = str(m['diagnostic_conflict'])\n"
runner=runner.replace(anchor,extra+anchor); compile(runner,str(c/'run.py'),'exec')
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6410-rank45-transitive','6415-rank45-schedule'))
m.update(authorities={a:dict(path=str(p.relative_to(root)),sha256=sha(p)) for a,p in authorities.items()},
    parent_summary=dict(path=str((old/'summary.json').relative_to(root)),sha256=sha(old/'summary.json')),
    parent_runs={a:str(old.relative_to(root)) for a in binaries},
    purpose='Distinguish actual input key-state from sort behavior for second76464 invocation: current source1523 versus predecessor/native91, before five missed binary reductions.',
    validation='Require every6410 ordinary+ALL census trace and each own partial-proof bytes/hash exact; parse all four snapshots per arm and compare full membership/keys/orders for both invocations.',
    test_consideration='Reused actual6350 writer/parsers and negative controls; new6413 full106Release105trace suites, ordinary Release byte-exact6357. No new semantics; whole-parent replay is observer-neutrality oracle.',
    limitation='Read-only diagnostics, not timing, full-graph equivalence, proof acceptance or policy promotion.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
