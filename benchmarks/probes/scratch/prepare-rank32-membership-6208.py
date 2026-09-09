#!/usr/bin/env python3
"""Capture full first-schedule membership while replaying every parent stream."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6194-rank32-selected-heaps'
build = root / 'benchmarks/runs/20260909-6206-schedule-continue-build/summary.json'
b = json.loads(build.read_text()); assert b['release_exact_parent']
s = json.loads((old / 'summary.json').read_text()); assert s['parent_replay']
for rel, files in b['sources'].items():
    for name, digest in files.items(): assert sha(root / rel / name) == digest
c = root / 'benchmarks/runs/20260909-6208-rank32-schedule-membership'
assert not c.exists()
for name in ('pins', 'traces', 'results', 'logs'): (c / name).mkdir(parents=True)
for arm, path in (('candidate', 'build/pin-6206-schedule-continue-trace/tactix'),
                  ('kissat', 'tools/solvers/variants/kissat-schedule-continue-6206/build/kissat')):
    assert sha(root / path) == b['binaries'][path]
    shutil.copy2(root / path, c / 'pins' / arm)
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest
runner = (old / 'run.py').read_text().replace('tactix-6194-', 'tactix-6208-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "env[prefix + '_BVE_SCHEDULE_TRACE'] = str(c / 'traces' / (arm + '.schedule.tsv'))\nenv['SAT_BVE_SCHEDULE_CONFLICT'] = '386968'\nenv['SAT_BVE_SCHEDULE_ORDINAL'] = '0'\nenv['SAT_BVE_SCHEDULE_MEMBERS'] = '1'\nenv['SAT_BVE_SCHEDULE_CONTINUE'] = '1'\n" + anchor)
compile(runner, str(c / 'run.py'), 'exec'); (c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6194-rank32', '6208-rank32'))
m.update(parent_summary=dict(path=str((old / 'summary.json').relative_to(root)), sha256=sha(old / 'summary.json')),
    observer_build=dict(path=str(build.relative_to(root)), sha256=sha(build)),
    purpose='Resolve incoming full BVE membership/score differences behind30834 versus30839 scheduled entries before attempt674.',
    hypothesis='Different initial eligibility/retry membership predates the selected pop; determine exact symmetric difference and shared-score differences before altering a producer.',
    validation='Changed diagnostic binaries must replay every complete6194 decision/prefix/boundary/macro/census/selected-heap stream and partialproof exactly. Only added schedule output may differ;the observer must continue through1M.',
    test_consideration='Release99/99 andtrace98/98;Release SHA exactly6107. Both actual schedule adapters have exit/return,conflict/ordinal,no-overwrite tests;corrected native fixture layout matches release. Failures and generatedmakefile path audit retained in6206 buildsummary. No solver semantic changes.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'pins/kissat', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
