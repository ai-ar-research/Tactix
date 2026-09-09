#!/usr/bin/env python3
"""Bind both current arms before interpreting the selected-pair discrepancy."""
import importlib.util
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from binary_lifecycle import read_lifecycle
from decision_prefix import read_prefix, validate_common, first_stream_difference

c = root / 'benchmarks/runs/20260909-6562-rank50-current-material-pair'
target = c / 'summary-6564.json'
assert not target.exists()
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    if expected is not None:
        assert digest == expected, p
    hashes[str(p.relative_to(root))] = digest
    return digest
def read(p, expected=None):
    bind(p, expected)
    return json.loads(p.read_text())

m = read(c / 'manifest.json')
launch = read(c / 'launch.json')
assert launch['manifest_sha256'] == sha(c / 'manifest.json')
for rel, digest in m['hashes'].items():
    bind(c / rel, digest)
for rel, digest in m['earlier_closure'].items():
    bind(root / rel, digest)
b = read(root / m['source_manifest'])
for rel, digest in b['files'].items():
    bind(root / b['source'] / rel, digest)
results, prefixes = {}, {}
for index, arm in enumerate(('candidate', 'candidate_control')):
    r = read(c / 'results' / (arm + '.json'))
    results[arm] = r
    assert r['arm'] == arm and r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == sha(c / 'pins' / arm)
    assert r['partial_certificate']['bytes'] > 0
    log = c / 'logs' / (launch['job_id'] + '_' + str(index) + '.log')
    bind(log)
    assert log.read_text().splitlines() == ['scratch=' + r['scratch'], arm + ' bounded stop and cleanup complete']
    assert '/tactix-6562-' + r['slurm_job'] + '-' in r['scratch']
    for name, digest in r['trace_sha256'].items():
        bind(c / 'traces' / name, digest)
    for suffix in ('decisions', 'boundaries', 'prefix'):
        assert arm + '.' + suffix + '.tsv' in r['trace_sha256']
    prefixes[arm] = read_prefix(c / 'traces' / (arm + '.prefix.tsv'))
    validate_common(prefixes[arm], stop=1000000, common=139633, digest='236dcfa1a362af4d')
assert results['candidate']['slurm_job'] != results['candidate_control']['slurm_job']
ordinary = lambda r: {n.split('.', 1)[1]: d for n, d in r['trace_sha256'].items() if not n.endswith('.lifecycle.tsv')}
assert ordinary(results['candidate']) == ordinary(results['candidate_control'])
assert results['candidate']['partial_certificate'] == results['candidate_control']['partial_certificate']
native = root / m['native_campaign']
ns = read(native / 'summary-6558.json')
nr = read(native / 'results/kissat.json')
assert ns['ordinary_replay_exact'] and nr['exit_code'] == 0 and nr['scratch_deleted']
for name, digest in nr['trace_sha256'].items():
    p = native / 'traces' / name
    bind(p, digest)
    assert ns['hashes'][str(p.relative_to(root))] == digest
lifecycle = {}
for arm, path, solver in (
    ('candidate', c / 'traces/candidate.lifecycle.tsv', 'tactix'),
    ('kissat', native / 'traces/kissat.lifecycle.tsv', 'kissat')):
    with path.open() as stream:
        lifecycle[arm] = read_lifecycle(stream, solver, {(-525, 574)})
assert lifecycle['kissat'] == ns['pair_lifecycle']
path = Path(__file__).with_name('localize-root-lock-rank37-6073.py')
spec = importlib.util.spec_from_file_location('boundaries6073', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.fixtures()
boundaries, ignored = {}, {}
for arm, campaign in (('candidate', c), ('kissat', native)):
    with (campaign / 'traces' / (arm + '.boundaries.tsv')).open() as stream:
        boundaries[arm], ignored[arm] = module.parse(stream, arm == 'kissat')
decision = first_stream_difference(c / 'traces/candidate.decisions.tsv', native / 'traces/kissat.decisions.tsv')
window = {arm: [r for r in rows if 780400 <= r['conflicts'] <= 823563] for arm, rows in boundaries.items()}
# Historical comparison is measured, not assumed from a different source.
old = root / 'benchmarks/runs/20260909-6286-rank50-admission'
old_result = read(old / 'results/candidate_control.json')
historical = dict(ordinary_exact=ordinary(results['candidate']) == ordinary(old_result),
                  partial_certificate_exact=results['candidate']['partial_certificate'] == old_result['partial_certificate'])
for rel in ('tools/bench/binary_lifecycle.py', 'tools/bench/decision_prefix.py',
            'benchmarks/probes/scratch/localize-root-lock-rank37-6073.py',
            'benchmarks/probes/scratch/validate-current-material-pair-6564.py'):
    bind(root / rel)
out = dict(hashes=hashes, observer_neutral=True, ordinary_trace_sha256=ordinary(results['candidate']),
    partial_certificate=results['candidate']['partial_certificate'], scratch_deleted=True,
    prefixes=prefixes, lifecycle=lifecycle, decision_difference=decision,
    boundary_window=window, boundary_comparison=module.compare(boundaries['candidate'], boundaries['kissat']),
    ignored_boundary_names=ignored, historical6286=historical,
    limitation='Bounded observer-neutral current-source witness, not final-proof acceptance or a runtime gain. Native/current event placement and value/copy metadata differ; no full upstream call-stack or formula identity is claimed. General material-tracker implementation remains unapproved.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps({k: out[k] for k in ('observer_neutral', 'lifecycle', 'decision_difference', 'boundary_window', 'historical6286')}, indent=2))
