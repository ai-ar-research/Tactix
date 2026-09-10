#!/usr/bin/env python3
"""Localize the observed Rank31 regression using both actual arms and cached native."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
old = root / 'benchmarks/runs/20260909-6611-material-rank31-early'
c = root / 'benchmarks/runs/20260910-7046-collect-retry-rank31'
assert not c.exists()
m = json.loads((old / 'manifest.json').read_text())
dependencies = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or expected == digest, p
    dependencies[str(p.relative_to(root))] = digest
for rel, digest in m['hashes'].items(): bind(old / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
native = root / m['cached_native']
nr = json.loads((native / 'results/kissat.json').read_text())
assert nr['exit_code'] == 0 and nr['scratch_deleted']
bind(native / 'pins/kissat', nr['binary_sha256'])
bind(native / 'results/kissat.json')
for name, digest in nr['trace_sha256'].items(): bind(native / 'traces' / name, digest)
gate = root / 'benchmarks/runs/20260910-7034-collect-retry-BC-gate/summary-7036.json'
g = json.loads(gate.read_text()); assert g['solver_records'] == 66
for rel, digest in g['hashes'].items(): bind(root / rel, digest)
runtime = root / 'benchmarks/runs/20260910-7041-collect-retry-runtime'
measurements = {}
for task in range(4):
    p = runtime / f'results/{task:03d}.json'
    if not p.exists(): p = runtime / f'provisional/{task:03d}.json'
    r = json.loads(p.read_text())
    assert classify(r, p.parent.name == 'results')['status'] in ('pending_validation', 'checked_UNSAT')
    bind(p); measurements[(r['rank'], r['arm'])] = r['solve']['wall_seconds']
assert measurements[(31, 'candidate')] > measurements[(31, 'previous')]
for name in ('pins', 'logs', 'results', 'traces'): (c / name).mkdir(parents=True)
for arm, date, stem in [('candidate', '20260910', '7030-collect-retry'),
                         ('previous', '20260909', '6730-reduce-watermark')]:
    path = root / f'benchmarks/runs/{date}-{stem}-build/manifest.json'
    b = json.loads(path.read_text()); bind(path)
    for rel, digest in b['files'].items(): bind(root / b['source'] / rel, digest)
    pin = root / f'build/pin-{stem}-trace/tactix'
    bind(pin, b['binaries'][str(pin.relative_to(root))])
    shutil.copy2(pin, c / 'pins' / arm)
runner = (old / 'run.py').read_text()
before = "('candidate',)[int(sys.argv[1])]"
assert runner.count(before) == 1
runner = runner.replace(before, "('candidate', 'previous')[int(sys.argv[1])]").replace('tactix-6611-', 'tactix-7046-')
(c / 'run.py').write_text(runner)
batch_source = root / 'benchmarks/runs/20260910-7038-collect-retry-prefix'
bind(batch_source / 'array.sbatch')
batch = (batch_source / 'array.sbatch').read_text().replace(str(batch_source), str(c)).replace(
    '7038-collect-retry-prefix', '7046-collect-retry-rank31').replace('--array=0-0', '--array=0-1')
(c / 'array.sbatch').write_text(batch)
for p in (old / 'manifest.json', gate, Path(__file__),
          root / 'benchmarks/runs/20260909-6611-material-rank31-early/summary-6613.json'):
    bind(p)
out = {key: m[key] for key in ('input', 'input_sha256_compressed', 'common_decisions', 'prior_common_hash', 'cached_native')}
out.update(rank=31, arms=['candidate', 'previous'], stop_conflict=300000, stream_from=0,
    dependencies=dependencies,
    resource_contract=json.loads((batch_source / 'manifest.json').read_text())['resource_contract'] | {'array': '0-1'},
    session_authority='Explicit user continue after Sep10 06:00EDT; largest gaps and adverse outcomes first.',
    hypothesis='First paired wave improves35 but regresses31 from483.891 to532.868s with more conflicts. Compare actual7030 versus6730 and cached native through300K, without assuming inherited prefix equality or restoring rejected5920. Locate any earlier collection-induced split, or preserve the known179823 omission if unchanged.',
    validation='Complete0..300K row accounting, bound source/pins/traces and clean partial-proof deletion. Compare every emitted decision pair and portable boundary context. No native rerun or full-run equality claim.',
    test_consideration='No solver/observer edit; reuse full-stream accounting, chunked comparison and boundary schema fixtures. Actual regression is an investigation trigger, not a source correctness incident.',
    limitation='First-wave performance is provisional; finish all predeclared timings. Do not restore preconnection cleanup merely for another matching early prefix; a retest needs evidence addressing the old late regression.')
out['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
