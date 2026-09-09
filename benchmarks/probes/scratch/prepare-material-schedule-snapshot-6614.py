#!/usr/bin/env python3
"""Bind current first-BVE physical scores to the retained 5862 witness."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

c = root / 'benchmarks/runs/20260909-6614-material-rank31-schedule'
assert not c.exists()
old = root / 'benchmarks/runs/20260908-5862-rank31-bve-frontier'
om = json.loads((old / 'manifest.json').read_text())
for rel, digest in om['hashes'].items():
    assert sha(old / rel) == digest, rel
build = root / 'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json'
b = json.loads(build.read_text())
for rel, digest in b['files'].items():
    assert sha(root / b['source'] / rel) == digest, rel
pin = root / 'build/pin-6579-material-schedule-trace/tactix'
assert sha(pin) == b['binaries'][str(pin.relative_to(root))]
control = root / 'benchmarks/runs/20260909-6611-material-rank31-early'
dependencies = {}
for p in (build, control / 'manifest.json', control / 'summary-6613.json',
          control / 'results/candidate.json', old / 'manifest.json',
          old / 'schedule-diagnosis.json',
          root / 'benchmarks/reports/2026-09-08-composition-runtime-and-formula.md'):
    dependencies[str(p.relative_to(root))] = sha(p)
for campaign, arms in ((old, ('candidate', 'kissat')), (control, ('candidate',))):
    for arm in arms:
        p = campaign / f'results/{arm}.json'
        r = json.loads(p.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted']
        dependencies[str(p.relative_to(root))] = sha(p)
        for rel, digest in r['trace_sha256'].items():
            p = campaign / 'traces' / rel
            assert sha(p) == digest, p
            dependencies[str(p.relative_to(root))] = digest
runner = (old / 'run.py').read_text()
changes = {
    "('candidate', 'kissat')[int(sys.argv[1])]": "('candidate',)[int(sys.argv[1])]",
    'tactix-5862-': 'tactix-6614-',
    "assert (c / 'traces' / (arm + '.prefix.tsv')).exists(), 'Requested stop was not observed'":
        "assert (c / 'traces' / (arm + '.bve-schedule.tsv')).exists(), 'Requested schedule stop was not observed'\nassert not (c / 'traces' / (arm + '.prefix.tsv')).exists(), 'Unexpected later stop'",
}
for before, after in changes.items():
    assert runner.count(before) == 1, before
    runner = runner.replace(before, after)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c))
batch = batch.replace('5862-bve-frontier', '6614-rank31-schedule').replace('--array=0-1', '--array=0')
for folder in ('pins', 'logs', 'traces', 'results'):
    (c / folder).mkdir(parents=True)
shutil.copy2(pin, c / 'pins/candidate')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text(batch)
protected = json.loads((root / 'benchmarks/reports/2026-09-09-material-followup-audit-6609.json').read_text())['protected_campaign_hashes']
m = dict(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    input=om['input'], input_sha256_compressed=om['input_sha256_compressed'],
    stop_conflict=300000, stream_from=0, common_decisions=1,
    actual_stop=dict(observer='BVE schedule', conflicts=179823, ordinal=0, before_first_candidate=True),
    candidate_authority=str(build.relative_to(root)), cached_control=str(control.relative_to(root)),
    cached_schedule=str(old.relative_to(root)), dependencies=dependencies,
    protected_campaign_hashes=protected, resource_contract=dict(partition='cpu-dense-hwulab-q',
        cpus_per_task=12, memory_mib=48000, solve_timeout=1200,
        scheduler_wall_seconds=1500, array='0', throttle=None, node_constraint=None),
    hypothesis='The current179823 split is the old preconnection occurrence-score witness, not a new heap-tie defect. Compare all current schedule members/positions/scores with retained5862 and native.',
    validation='Require intentional schedule stop, clean exit and scratch, full emitted decision prefix against6611, complete schedule framing and every member. No later300K stop or proof acceptance expected.',
    test_consideration='No solver or observer edit. Reuse fixed observer; correct old runner stop assertion. Validator must test duplicate/missing positions, schema, and missing member rows.',
    limitation='An exact old score witness would localize a known omitted repair, not justify reviving5920: its Rank31 and C regressions remain controlling adverse evidence. No fresh native run, source edit, fuzz access, promotion or timing claim.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'run.py', 'array.sbatch')}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
    out.write('\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)
