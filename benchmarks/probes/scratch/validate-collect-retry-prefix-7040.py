#!/usr/bin/env python3
"""Validate repaired prefix against frozen current/native, retaining earlier drift."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_common, validate_suffix_accounting, first_stream_difference
c = root / 'benchmarks/runs/20260910-7038-collect-retry-prefix'
target = c / 'summary-7040.json'
assert not target.exists()
result = c / 'results/candidate.json'
if not result.exists():
    print('Bounded candidate still pending; no summary written'); sys.exit(0)
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
for rel, digest in m['hashes'].items(): bind(c / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
for p in (c / 'manifest.json', c / 'launch.json', result, Path(__file__)): bind(p)
r = json.loads(result.read_text())
assert r['exit_code'] == 0 and r['arm'] == 'candidate'
assert r['scratch_deleted'] and not Path(r['scratch']).exists()
assert r['binary_sha256'] == m['hashes']['pins/candidate']
assert r['partial_certificate']['bytes'] > 0
assert set(r['trace_sha256']) == {'candidate.' + suffix + '.tsv' for suffix in ('prefix', 'decisions', 'boundaries', 'macro')}
for name, digest in r['trace_sha256'].items(): bind(c / 'traces' / name, digest)
p = read_prefix(c / 'traces/candidate.prefix.tsv')
assert p['solver'] == 'tactix' and p['conflict'] == m['stop_conflict']
assert p['common'] == m['common_decisions']
inherited = p['common_reached'] == 1 and p['common_hash'] == m['prior_common_hash']
comparisons = {}
parent = root / m['parent']
old = json.loads((parent / 'summary-7015.json').read_text())
for rel, digest in old['hashes'].items(): bind(root / rel, digest)
if inherited:
    validate_common(p, stop=m['stop_conflict'], common=m['common_decisions'], digest=m['prior_common_hash'])
    stream = c / 'traces/candidate.decisions.tsv'
    count = int(subprocess.check_output(['wc', '-l', stream], text=True).split()[0])
    first = list(map(int, subprocess.check_output(['head', '-n', '1', stream], text=True).split()))
    last = list(map(int, subprocess.check_output(['tail', '-n', '1', stream], text=True).split()))
    validate_suffix_accounting(p, first, last, count, common_count=m['common_decisions'], overlap_count=0, stream_from=m['stream_from'])
    for arm in ('candidate', 'kissat'):
        comparison = first_stream_difference(stream, parent / 'traces' / (arm + '.decisions.tsv'))
        comparison['matching_rows_from_start'] = m['common_decisions'] + comparison['matching_rows']
        comparisons[arm] = comparison
out = dict(hashes=hashes, record=r, prefix=p, inherited100k=inherited, comparisons=comparisons,
    decision='Localize the preceding BVE boundary if native divergence remains; extend if equal. If inherited100K changed, localize earlier before making any late closure claim.',
    limitation='Changed-source bounded probe, not observer neutrality, final proof acceptance or a speedup. No requirement that a longer native prefix improves runtime; repeated timings still decide.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps({key: out[key] for key in ('inherited100k', 'comparisons')}, indent=2))
