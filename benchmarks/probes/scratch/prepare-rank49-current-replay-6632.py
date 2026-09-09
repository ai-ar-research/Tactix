#!/usr/bin/env python3
"""Revalidate a short added-rank witness with current source, no native rerun."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
old = root / 'benchmarks/runs/20260909-6201-rank49-current-prefix'
c = root / 'benchmarks/runs/20260909-6632-rank49-current-replay'
assert not c.exists()
m = json.loads((old / 'manifest.json').read_text())
dependencies = {}
def bind(p, expected=None):
    digest = sha(p)
    if expected is not None:
        assert digest == expected, p
    dependencies[str(p.relative_to(root))] = digest
for rel, digest in m['hashes'].items():
    bind(old / rel, digest)
for name in ('manifest.json', 'summary.json', 'launch.json'):
    bind(old / name)
for arm in ('candidate', 'kissat'):
    p = old / f'results/{arm}.json'; bind(p)
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    for rel, digest in r['trace_sha256'].items():
        bind(old / 'traces' / rel, digest)
build = root / 'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json'
bind(build)
b = json.loads(build.read_text())
candidate = root / 'build/pin-6579-material-schedule-trace/tactix'
assert sha(candidate) == b['binaries'][str(candidate.relative_to(root))]
for rel, digest in b['files'].items():
    assert sha(root / b['source'] / rel) == digest, rel
current = root / 'benchmarks/runs/20260909-6594-material-ranks1-50/results/048.json'
earlier = root / 'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50/results/048.json'
for p in (current, earlier):
    bind(p)
    status = classify(json.loads(p.read_text()), True)
    assert status['checked'] and status['clean']
a, z = [json.loads(p.read_text()) for p in (current, earlier)]
nt = lambda r: [s for s in r['solver_statistics'] if not s.startswith('c timing')]
assert a['certificate_sha256'] == z['certificate_sha256']
full_comparison = dict(certificate_equal=True, non_timing_stats_equal=nt(a) == nt(z),
    earlier_only_statistics=[s for s in nt(z) if s not in nt(a)],
    current_only_statistics=[s for s in nt(a) if s not in nt(z)],
    preflight='Initial full-work equality assertion rejected before campaign creation: BVE resolutions/definition work and material-touch counters differ. Full certificate identity is not full work identity. Retain these differences and test current prefix applicability directly.')
for name in ('pins', 'logs', 'results', 'traces'):
    (c / name).mkdir(parents=True)
shutil.copy2(candidate, c / 'pins/candidate')
runner = (old / 'run.py').read_text().replace("('candidate', 'kissat')[int(sys.argv[1])]", "('candidate',)[int(sys.argv[1])]")
runner = runner.replace('tactix-6201-', 'tactix-6632-')
assert "('candidate',)[int(sys.argv[1])]" in runner
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6201-rank49-prefix', '6632-rank49-replay').replace('--array=0-1', '--array=0-0')
(c / 'array.sbatch').write_text(batch)
m['resource_contract']['array'] = '0-0'
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), cached_controls=str(old.relative_to(root)),
    candidate_authority=dict(path=str(build.relative_to(root)), sha256=sha(build)),
    purpose='Current6579 full49 certificate matches semantic6107 but BVE/definition/material-touch counters differ. Replay all100K observed prefix traces/partial proof against6201 before reusing its early split. No native solver invocation or denominator refresh.',
    full_control_comparison=full_comparison,
    hypothesis='Rank49 still contributes136.060s excess; if current6201 replay is exact, localize its earlier first conflict/analysis drift at9273 rather than infer cause from Congruence binary counts at2988.',
    validation='Exact complete per-arm6201 candidate trace/partial-proof replay at100K is the applicability criterion. A mismatch changes the diagnostic premise, not automatically solver correctness; localize it before causal interpretation.',
    test_consideration='No solver or observer edits. Existing complete-prefix readers and tests cover replay; independent Rank35 input-reader additions have their own tests. Current6579 fixed110/110/109 suites and B/C6584 checked gate are retained.',
    dependencies=dependencies,
    protected_campaign_hashes=json.loads((root / 'benchmarks/reports/2026-09-09-rank35-reorder-audit-6627.json').read_text())['protected_campaign_hashes'])
m['hashes'] = {name: sha(c / name) for name in ('pins/candidate', 'run.py', 'array.sbatch')}
with (c / 'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2); stream.write('\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)
