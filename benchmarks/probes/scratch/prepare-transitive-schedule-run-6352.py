#!/usr/bin/env python3
"""Predeclare exact-parent replay and before/after scheduler-key comparison."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6345-rank32-selected-transitive'
summary=json.loads((old/'summary.json').read_text());assert summary['parent_replay']
for rel,digest in summary['hashes'].items():assert sha(root/rel)==digest
authority=root/'benchmarks/runs/20260909-6350-transitive-schedule-build/manifest.json'
build=json.loads(authority.read_text());assert build['release_exact_parent']
for source,files in build['files'].items():
    for rel,digest in files.items():assert sha(root/source/rel)==digest
for rel,digest in build['binaries'].items():assert sha(root/rel)==digest
c=root/'benchmarks/runs/20260909-6352-rank32-transitive-schedule';assert not c.exists()
for name in ('pins','traces','results','logs'):(c/name).mkdir(parents=True)
for arm,binary in (('candidate','build/pin-6350-transitive-schedule-trace/tactix'),
                   ('kissat','tools/solvers/variants/kissat-transitive-schedule-6350/build/kissat')):
    shutil.copy2(root/binary,c/'pins'/arm)
m=json.loads((old/'manifest.json').read_text())
runner=(old/'run.py').read_text().replace('tactix-6345-','tactix-6352-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)";assert runner.count(anchor)==1
runner=runner.replace(anchor,"env['SAT_TRANSITIVE_SCHEDULE_PATH'] = str(c / 'traces' / (arm + '.schedule.tsv'))\nenv['SAT_TRANSITIVE_SCHEDULE_CONFLICT'] = str(m['diagnostic_conflict'])\n"+anchor)
compile(runner,str(c/'run.py'),'exec');(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6345-transitive-pair','6352-transitive-schedule'))
binding=dict(path=str(authority.relative_to(root)),sha256=sha(authority))
m.update(candidate_authority=binding,native_authority=binding,
    parent_runs={a:str(old.relative_to(root)) for a in ('candidate','kissat')},
    parent_summary=dict(path=str((old/'summary.json').relative_to(root)),sha256=sha(old/'summary.json')),
    hypothesis='Both endpoints are active/unfixed but Tactix omits936. Compare actual full compact source membership, comparator-key identities/score bits/stamps, and before/after order; distinguish input-state difference from sort implementation or downstream effort cutoff.',
    validation='Require all6345 parent ordinary+selected traces and partial proof bytes/hash exact per arm through3M despite changed diagnostic binary. Only additional trace is schedule.tsv. Validate complete snapshot framing and before/after multiset preservation before comparison; report every invocation at1368343.',
    test_consideration='6350 Release byte-identical to6305,105Release104trace; actual shared C/C++ diagnostic writer fixtures identical. Add schedule parser/framing controls before interpreting real traces; no new semantic gate or source promotion.',
    scope='Fresh6350 read-only scheduler adapters on6305/6090; no sort/comparator/eligibility/budget/API/proof changes.',
    limitation='Diagnostic-only; raw compact keys include reference IDX-on-variable behavior. Tactix priority is0 because comparator does not consume a priority flag. No timing/final proof acceptance or inferred new primitive approval.')
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
