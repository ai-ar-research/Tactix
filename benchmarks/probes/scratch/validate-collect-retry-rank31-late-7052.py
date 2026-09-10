#!/usr/bin/env python3
"""Account for actual threshold overlap before locating the later paired split."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_common, validate_suffix_accounting, first_stream_difference
c = root / 'benchmarks/runs/20260910-7050-collect-retry-rank31-late'
target = c / 'summary-7052.json'
assert not target.exists()
if any(not (c / 'results' / (a + '.json')).exists() for a in ('candidate', 'previous')):
    print('Rank31 later diagnostic pending; no summary written'); sys.exit(0)
m = json.loads((c / 'manifest.json').read_text()); parent = root / m['parent']
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or expected == digest, p
    hashes[str(p.relative_to(root))] = digest
for rel, digest in m['hashes'].items(): bind(c / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
reader = Path(__file__).with_name('localize-root-lock-rank37-6073.py')
spec = importlib.util.spec_from_file_location('boundaries', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
for p in (c / 'manifest.json', c / 'launch.json', reader, Path(__file__)): bind(p)
old = json.loads((parent / 'summary-7048.json').read_text())
assert old['comparisons']['candidate_vs_previous']['decisions']['complete_equal']
old_stream = parent / 'traces/candidate.decisions.tsv'
# Filtering the known complete old stream avoids assuming one overlap row or
# putting an arbitrary cap on the number of decisions at the threshold.
overlap = subprocess.check_output(['awk', '-F', '\t', '-v',
    'cutoff=' + str(m['stream_from']), '$1 == cutoff {print}', str(old_stream)])
if overlap:
    assert overlap.endswith(b'\n')
    assert subprocess.check_output(['tail', '-c', str(len(overlap)), old_stream]) == overlap
overlap_count = overlap.count(b'\n')
prefixes, streams, projected, inherited = {}, {}, {}, {}
for arm in ('candidate', 'previous'):
    path = c / 'results' / (arm + '.json'); bind(path)
    r = json.loads(path.read_text())
    assert r['exit_code'] == 0 and r['arm'] == arm and r['partial_certificate']['bytes'] > 0
    assert r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256'] == m['hashes']['pins/' + arm]
    for name, digest in r['trace_sha256'].items(): bind(c / 'traces' / name, digest)
    prior = read_prefix(parent / 'traces' / (arm + '.prefix.tsv'))
    assert prior['hash'] == m['prior_common_hash'] and prior['count'] == m['common_decisions']
    p = read_prefix(c / 'traces' / (arm + '.prefix.tsv'))
    assert p['conflict'] == m['stop_conflict'] and p['common'] == m['common_decisions']
    inherited[arm] = p['common_reached'] == 1 and p['common_hash'] == m['prior_common_hash']
    stream = c / 'traces' / (arm + '.decisions.tsv')
    if inherited[arm]:
        validate_common(p, stop=m['stop_conflict'], common=m['common_decisions'], digest=m['prior_common_hash'])
        with stream.open('rb') as inp: assert inp.read(len(overlap)) == overlap
        count = int(subprocess.check_output(['wc', '-l', stream], text=True).split()[0])
        first = list(map(int, subprocess.check_output(['head', '-n', '1', stream], text=True).split()))
        last = list(map(int, subprocess.check_output(['tail', '-n', '1', stream], text=True).split()))
        validate_suffix_accounting(p, first, last, count, common_count=m['common_decisions'],
            overlap_count=overlap_count, stream_from=m['stream_from'])
    prefixes[arm], streams[arm] = p, stream
    with (c / 'traces' / (arm + '.boundaries.tsv')).open() as inp:
        projected[arm], ignored = h.parse(inp, False)
comparison, near = None, {}
if all(inherited.values()):
    comparison = first_stream_difference(streams['candidate'], streams['previous'])
    comparison['matching_rows_from_start'] = m['common_decisions'] + comparison['matching_rows'] - overlap_count
    for arm, key in [('candidate', 'left'), ('previous', 'right')]:
        row = comparison[key]
        if row is not None: near[arm] = [r for r in projected[arm] if r['conflicts'] <= row[0]][-16:]
with target.open('x') as stream:
    json.dump(dict(hashes=hashes, prefixes=prefixes, inherited300k=inherited,
        overlap_rows=overlap_count, decisions=comparison, preceding_boundaries=near,
        limitation='Complete paired suffix with actual overlap bytes. Earlier hash drift takes priority. The old native179823 mismatch is distinct; this locates a new adverse transfer, not a formula-strength verdict or justification to restore5920.'), stream, indent=2)
    stream.write('\n')
print(json.dumps(dict(inherited300k=inherited, overlap_rows=overlap_count, decisions=comparison), indent=2))
