#!/usr/bin/env python3
"""Find the later differing phase without another huge complete decision stream."""
import csv
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260910-7064-rank31-full-boundaries'
assert not c.exists()
assert not any(k.startswith('SAT_') for k in os.environ), 'unexpected inherited diagnostic setting'
old = root / 'benchmarks/runs/20260910-7006-transitive-largest-gap-ablation'
runtime = root / 'benchmarks/runs/20260910-7041-collect-retry-runtime'
prefix = root / 'benchmarks/runs/20260910-7055-collect-retry-rank31-2m'
om = json.loads((old / 'manifest.json').read_text())
rm = json.loads((runtime / 'manifest.json').read_text())
ps = json.loads((prefix / 'summary-7057.json').read_text())
assert all(ps['inherited1m'].values()) and ps['decisions']['complete_equal']
assert ps['decisions']['matching_rows_from_start'] == 153799751
deps = {}
def bind(p, expected=None):
    value = sha(p); assert expected is None or value == expected, p
    deps[str(p.relative_to(root))] = value
for rel, digest in om['pin_sha256'].items(): bind(old / rel, digest)
for p in (old / 'manifest.json', runtime / 'manifest.json', prefix / 'manifest.json',
          prefix / 'summary-7057.json', runtime / 'solver-checkpoint-7044.json', Path(__file__)):
    bind(p)
for suffix in ('macro.tsv','boundaries.tsv'):
    a,b = (prefix / 'traces' / (arm + '.' + suffix) for arm in ('candidate','previous'))
    bind(a); bind(b); assert sha(a) == sha(b)
for p in (runtime / 'results').glob('*.json'):
    assert classify(json.loads(p.read_text()), True)['checked'], p
for name in ('pins','logs','results','provisional','claims','traces'):
    (c / name).mkdir(parents=True)
for name in ('run.py','runtime_task.py','solver_environment.py','checkmodel','modelcheck.py','drat-trim'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
arms = {}; oracles = {}; rows = []
for task, (arm, buildname, pinname) in enumerate((
    ('candidate','20260910-7030-collect-retry-build','pin-7030-collect-retry-trace'),
    ('previous','20260909-6730-reduce-watermark-build','pin-6730-reduce-watermark-trace'))):
    authority = root / 'benchmarks/runs' / buildname
    am = json.loads((authority / 'manifest.json').read_text()); bind(authority / 'manifest.json')
    for rel, digest in am['files'].items(): bind(root / am['source'] / rel, digest)
    binary = root / 'build' / pinname / 'tactix'
    bind(binary, am['binaries'][str(binary.relative_to(root))])
    shutil.copy2(binary, c / 'pins' / arm)
    environment = {'TACTIX_MACRO_BOUNDARY_TRACE':str(c / 'traces' / (arm + '.macro.tsv')),
                   'TACTIX_INPROCESS_BOUNDARY_TRACE':str(c / 'traces' / (arm + '.boundaries.tsv'))}
    arms[arm] = dict(rm['arms'][arm], path='pins/' + arm, sha256=sha(binary),
                     source_build='build/' + pinname, environment=environment)
    p = runtime / f'provisional/{task:03d}.json'; bind(p)
    r = json.loads(p.read_text()); assert r['rank'] == 31 and r['arm'] == arm
    assert classify(r, False)['status'] == 'pending_validation'
    oracles['31/' + arm] = dict(path=str(p.relative_to(root)), sha256=sha(p))
    rows.append(dict(task=task,rank=31,arm=arm,tier='D',instance=r['instance'],expected='UNSAT'))
with (c / 'matrix.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, rows[0], delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (old / 'array.sbatch').read_text().replace(str(old),str(c))
assert batch.count('--array=0-7') == 1
batch = batch.replace('--array=0-7','--array=0-1').replace('7006-transitive-ablation','7064-rank31-boundaries')
(c / 'array.sbatch').write_text(batch)
m = dict(schema='tactix-full-macro-boundary-diagnostic-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms, input_sha256={rows[0]['instance']:om['input_sha256'][rows[0]['instance']]},
    collect_runtime_context=True, resource_contract=dict(om['resource_contract'],array='0-1'),
    dependencies=deps, protected_campaign_hashes=om['protected_campaign_hashes'],
    unprofiled_records=oracles, selection=[dict(rank=31)], session_authority=om['session_authority'],
    purpose='The changed solver first diverges after2M conflicts. Capture the complete phase/CDCL macro skeleton with no decision stream or diagnostic stop; then target only the first differing phase.',
    hypothesis='A later collection retry effect changes a BVE/Strengthen frontier or its downstream search. Full common2M macro/boundary replay and full own release work/certificate replay are mandatory before localizing the later boundary.',
    validation='Expected full UNSAT, exact whole own non-timing statistics and certificate bytes/hash versus7041 Release, complete cached2M boundary/macro prefix replay, hash-bound raw traces. Classify any abnormal exit or rejection before further dependent work. Independent proof checks/cleanup remain asynchronous.',
    test_consideration='No source, observer or environment-wrapper behavior changed. Existing environment isolation, own-repeat and boundary fixtures cover the selected configuration. No new solver unit tests; no SAT_* deadline/event or decision stream controls enabled.',
    limitation='Full-run causal diagnostic, not an acceptance timing or native baseline. Trace counts alone do not establish formula identity. Do not restore rejected5920 for the old native179823 mismatch.')
m['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in [*(c / 'pins').iterdir(),c / 'matrix.tsv',c / 'array.sbatch']}
for rel, digest in m['protected_campaign_hashes'].items(): assert sha(root / rel) == digest, rel
with (c / 'manifest.json').open('x') as stream: json.dump(m, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
