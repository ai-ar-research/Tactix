#!/usr/bin/env python3
"""Validate cached overlap and every later row before localizing a first split."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_common, first_stream_difference
spec = importlib.util.spec_from_file_location('boundaries', Path(__file__).with_name('localize-root-lock-rank37-6073.py'))
bounds = importlib.util.module_from_spec(spec); spec.loader.exec_module(bounds)
bounds.fixtures()
c = root / 'benchmarks/runs/20260909-6183-rank32-current-prefix'
assert not (c / 'summary.json').exists()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest, rel
for rel, digest in m['earlier_closure'].items():
    assert sha(root / rel) == digest, rel
cached = root / 'benchmarks/runs/20260908-6060-root-lock-rank32-prefix'
old = json.loads((cached / 'summary.json').read_text())
stream = cached / 'traces/candidate.decisions.tsv'
assert sha(stream) == old['complete_trace_sha256'][stream.name]
assert old['matching_decisions'] == m['common_decisions'] == 69828932
# Both source filters include decisions at the threshold; retain and validate
# the entire overlap instead of dropping the first row or guessing its length.
overlap_count = int(subprocess.check_output(['rg', '-c', '^300000\t', str(stream)], text=True))
overlap = subprocess.check_output(['tail', '-n', str(overlap_count), str(stream)])
assert overlap_count > 0 and all(s.startswith(b'300000\t') for s in overlap.splitlines())
assert int(overlap.splitlines()[-1].split(b'\t')[3]) == old['new_prefix']['last']
prefixes = {}; projected = {}; ignored = {}; counts = {}; hashes = {}
for name in ('manifest.json', 'launch.json'):
    hashes[str((c / name).relative_to(root))] = sha(c / name)
for arm in ('candidate', 'kissat'):
    p = c / 'results' / (arm + '.json'); r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
    assert r['binary_sha256'] == sha(c / 'pins' / arm)
    assert r['partial_certificate']['bytes'] > 0
    hashes[str(p.relative_to(root))] = sha(p)
    for name, digest in r['trace_sha256'].items():
        p = c / 'traces' / name
        assert sha(p) == digest, p
        hashes[str(p.relative_to(root))] = digest
    prefix = read_prefix(c / 'traces' / (arm + '.prefix.tsv'))
    validate_common(prefix, stop=1000000, common=69828932, digest='10d4b4ca139060f5')
    assert prefix['common_last'] == old['new_prefix']['last']
    stream = c / 'traces' / (arm + '.decisions.tsv')
    with stream.open('rb') as inp:
        assert inp.read(len(overlap)) == overlap, 'cached/current overlap changed'
    count = int(subprocess.check_output(['wc', '-l', str(stream)], text=True).split()[0])
    assert count == prefix['count'] - m['common_decisions'] + overlap_count
    last = list(map(int, subprocess.check_output(['tail', '-n', '1', str(stream)], text=True).split('\t')))
    assert len(last) == 5 and last[0] <= m['stop_conflict'] and last[3] == prefix['last']
    assert last[1] == prefix['end_decisions']
    assert prefix['start_decisions'] == (prefix['end_decisions'] - prefix['count']) % (1 << 64)
    prefixes[arm], counts[arm] = prefix, count
    with (c / 'traces' / (arm + '.boundaries.tsv')).open() as inp:
        projected[arm], ignored[arm] = bounds.parse(inp, arm == 'kissat')
difference = first_stream_difference(c / 'traces/candidate.decisions.tsv', c / 'traces/kissat.decisions.tsv')
search = {arm: [r for r in rows if r['conflicts'] > 0] for arm, rows in projected.items()}
near = {}
if not difference['complete_equal']:
    for arm, key in (('candidate', 'left'), ('kissat', 'right')):
        conflict = difference[key][0]
        near[arm] = [r for r in projected[arm] if r['conflicts'] <= conflict][-12:]
result = dict(rank=32, stop=1000000, prefixes=prefixes, suffix_rows=counts,
    validated_overlap_rows=overlap_count, decisions=difference,
    matching_decisions_from_start=m['common_decisions'] + difference['matching_rows'] - overlap_count,
    boundaries_all=bounds.compare(projected['candidate'], projected['kissat']),
    boundaries_search=bounds.compare(search['candidate'], search['kissat']),
    last_boundaries_before_first_decision_difference=near, ignored_boundary_names=ignored, hashes=hashes,
    limitation='First selected decision split after a validated cached prefix,not its causal explanation or full-solve identity. Portable count boundaries do not prove formula,heap,effort parity. Both partial certificates were deleted; no final proof or timing claim.')
(c / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k: result[k] for k in ('decisions', 'matching_decisions_from_start', 'validated_overlap_rows', 'boundaries_search', 'last_boundaries_before_first_decision_difference')}, indent=2))
