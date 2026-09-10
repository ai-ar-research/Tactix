#!/usr/bin/env python3
"""Short next-largest UNSAT witness: full native/current decision localization."""
import csv
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha, reference, REFERENCE
from native_work import primary_statistics
c = root / 'benchmarks/runs/20260910-7077-rank43-full-decisions'
old = root / 'benchmarks/runs/20260910-7064-rank31-full-boundaries'
native = root / 'benchmarks/runs/20260907-5433-rank35-inprocess-prefix'
assert not c.exists()
assert not any(k.startswith(('SAT_', 'TACTIX_', 'KISSAT_')) for k in os.environ)
m = json.loads((old / 'manifest.json').read_text())
nm = json.loads((native / 'manifest.json').read_text())
ref = next(r for r in reference()['records'] if r['rank'] == 43)
cp = root / 'benchmarks/runs/20260910-7053-collect-retry-ranks1-50/provisional/042.json'
np = root / ref['source_campaign'] / 'results/005.json'
cr, nr = (json.loads(p.read_text()) for p in (cp, np))
assert cr['rank'] == nr['rank'] == 43 and cr['instance'] == nr['instance'] == ref['instance']
assert classify(cr, False)['status'] == 'pending_validation'
assert classify(nr, True)['checked'] and classify(nr, True)['clean']
assert primary_statistics(nr['solver_statistics'])['eliminated'] == 0
deps = {}
def bind(p, expected=None):
    value = sha(p); assert expected is None or value == expected, p
    deps[str(p.relative_to(root))] = value
for rel, digest in m['pin_sha256'].items(): bind(old / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
for rel, digest in nm['hashes'].items(): bind(native / rel, digest)
for p in (Path(__file__), old / 'manifest.json', native / 'manifest.json', cp, np, REFERENCE,
          root / 'tools/bench/native_work.py', root / 'tools/bench/test_native_work.py',
          root / 'benchmarks/reports/2026-09-10-collect-retry-full50-7062/comparison.tsv'):
    bind(p)
bind(root / 'benchmarks/cache' / ref['instance'], ref['input_sha256'])
for name in ('pins', 'logs', 'results', 'provisional', 'claims', 'traces'):
    (c / name).mkdir(parents=True)
for name in ('run.py', 'runtime_task.py', 'solver_environment.py', 'checkmodel', 'modelcheck.py', 'drat-trim'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(old / 'pins/candidate', c / 'pins/candidate')
shutil.copy2(native / 'pins/kissat', c / 'pins/kissat')
native_arm = json.loads((root / ref['source_campaign'] / 'manifest.json').read_text())['arms']['kissat']
arms = {}; oracles = {}; rows = []
for task, (arm, config, oracle) in enumerate((('candidate', m['arms']['candidate'], cp), ('kissat', native_arm, np))):
    prefix = 'TACTIX' if arm == 'candidate' else 'KISSAT'
    binary = c / 'pins' / arm
    for key in ('_DECISION_STREAM_TRACE', '_INPROCESS_BOUNDARY_TRACE'):
        assert (prefix + key).encode() + b'\0' in binary.read_bytes(), (arm, key)
    env = {prefix + '_DECISION_STREAM_TRACE': str(c / 'traces' / (arm + '.decisions.tsv')),
           prefix + '_INPROCESS_BOUNDARY_TRACE': str(c / 'traces' / (arm + '.boundaries.tsv'))}
    if arm == 'candidate':
        env['TACTIX_MACRO_BOUNDARY_TRACE'] = str(c / 'traces/candidate.macro.tsv')
    arms[arm] = dict(config, path='pins/' + arm, sha256=sha(binary), environment=env)
    oracles['43/' + arm] = dict(path=str(oracle.relative_to(root)), sha256=sha(oracle))
    rows.append(dict(task=task, rank=43, arm=arm, tier='D', instance=ref['instance'], expected='UNSAT'))
with (c / 'matrix.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, rows[0], delimiter='\t', lineterminator='\n'); writer.writeheader(); writer.writerows(rows)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c))
assert batch.count('7064-rank31-boundaries') == 1
(c / 'array.sbatch').write_text(batch.replace('7064-rank31-boundaries', '7077-rank43-decisions'))
m.update(schema='tactix-native-full-decision-diagnostic-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms, dependencies=deps, input_sha256={ref['instance']:ref['input_sha256']},
    unprofiled_records=oracles, selection=[dict(rank=43)],
    purpose='Locate the first current/native decision split on the next-largest short UNSAT gap while the Rank31 priority probe runs.',
    hypothesis='Native finishes before any BVE/Factor, while current performs both later. Find the earlier decision/inprocessing divergence before labeling the extra work a scheduling defect.',
    validation='Expected full UNSAT and own certificate bytes/hash; candidate complete non-timing statistics and native complete primary statistics section exact against their own frozen Release records. Validate full decision framing and terminal counters before first-difference and projected boundary interpretation. Independent proofs/cleanup remain asynchronous.',
    test_consideration='No solver/observer/wrapper changes. Five native-statistics parser tests cover complete section, duplicate/malformed/missing/truncated rows, rate/resource exclusion and changed work; existing decision and boundary controls reused.',
    limitation='Missing-information native diagnostic, not a fixed-denominator refresh or timing acceptance. Native macro trace unsupported and not requested. No SAT_* stop/filter variables, global trace, new default, source promotion or routine native rerun.')
m['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in
    [*(p for p in (c / 'pins').iterdir() if p.is_file()), c / 'matrix.tsv', c / 'array.sbatch']}
for rel, digest in m['protected_campaign_hashes'].items(): assert sha(root / rel) == digest, rel
with (c / 'manifest.json').open('x') as stream: json.dump(m, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
