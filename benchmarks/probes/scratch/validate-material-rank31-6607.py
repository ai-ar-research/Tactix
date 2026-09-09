#!/usr/bin/env python3
"""Bind full traces before interpreting current-vs-cached-native Rank31."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_common, validate_suffix_accounting, first_stream_difference
c = root / 'benchmarks/runs/20260909-6605-material-rank31-prefix'
target = c / 'summary-6607.json'; assert not target.exists(), 'Frozen summary exists'
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
for rel, digest in m['dependencies'].items(): assert sha(root / rel) == digest, rel
native = root / m['cached_native']; cached = root / m['cached_common']
hashes = {str((c / p).relative_to(root)): sha(c / p) for p in ('manifest.json', 'launch.json')}
prefixes = {}; streams = {}; rows = {}; edges = {}
for arm, directory in (('candidate', c), ('kissat', native)):
    p = directory / 'results' / (arm + '.json'); r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
    assert r['binary_sha256'] == sha(directory / 'pins' / arm)
    assert r['partial_certificate']['bytes'] > 0
    hashes[str(p.relative_to(root))] = sha(p)
    for rel, digest in r['trace_sha256'].items():
        p = directory / 'traces' / rel; assert sha(p) == digest, p
        hashes[str(p.relative_to(root))] = digest
    prefix = read_prefix(directory / 'traces' / (arm + '.prefix.tsv'))
    assert prefix['conflict'] == m['stop_conflict'] and prefix['common'] == m['common_decisions']
    stream = directory / 'traces' / (arm + '.decisions.tsv')
    count = int(subprocess.check_output(['wc', '-l', str(stream)], text=True).split()[0])
    with stream.open('rb') as inp:
        first = list(map(int, inp.readline().split(b'\t')))
        inp.seek(-1, 2); assert inp.read(1) == b'\n', 'Truncated decision stream'
    last = list(map(int, subprocess.check_output(['tail', '-n', '1', str(stream)], text=True).split('\t')))
    prefixes[arm] = prefix; streams[arm] = stream; rows[arm] = count; edges[arm] = (first, last)

# Native emits no decision at exactly300K, so the retained stream has zero
# overlap. This is checked against both the earlier prefix and the stream,
# not assumed from the threshold setting or a one-row shortcut.
prior = read_prefix(cached / 'traces/kissat.prefix.tsv')
assert prior['conflict'] == m['stream_from'] and prior['count'] == m['common_decisions']
assert prior['hash'] == m['prior_common_hash'] and edges['kissat'][0][0] > m['stream_from']
validate_common(prefixes['kissat'], stop=m['stop_conflict'], common=m['common_decisions'], digest=m['prior_common_hash'])
validate_suffix_accounting(prefixes['kissat'], *edges['kissat'], rows['kissat'],
    common_count=m['common_decisions'], overlap_count=0, stream_from=m['stream_from'])
current = prefixes['candidate']
out = dict(rank=31, stop=m['stop_conflict'], prefixes=prefixes, suffix_rows=rows, hashes=hashes,
    limitation='First selected decision localization, not causation or full formula/effort/solve identity. Partial proofs hashed/deleted, not checked final certificates. Cached native is not a timing reference refresh.')
if current['common_reached'] != 1 or current['common_hash'] != m['prior_common_hash']:
    out.update(status='earlier_prefix_changed', next='Localize current source before300K; do not attribute the old394397 split.')
else:
    validate_common(current, stop=m['stop_conflict'], common=m['common_decisions'], digest=m['prior_common_hash'])
    validate_suffix_accounting(current, *edges['candidate'], rows['candidate'],
        common_count=m['common_decisions'], overlap_count=0, stream_from=m['stream_from'])
    difference = first_stream_difference(streams['candidate'], streams['kissat'])
    spec = importlib.util.spec_from_file_location('boundaries', Path(__file__).with_name('localize-root-lock-rank37-6073.py'))
    bounds = importlib.util.module_from_spec(spec); spec.loader.exec_module(bounds); bounds.fixtures()
    projected = {}; ignored = {}
    for arm, directory in (('candidate', c), ('kissat', native)):
        with (directory / 'traces' / (arm + '.boundaries.tsv')).open() as inp:
            projected[arm], ignored[arm] = bounds.parse(inp, arm == 'kissat')
    near = {}
    if not difference['complete_equal']:
        for arm, key in (('candidate', 'left'), ('kissat', 'right')):
            row = difference[key]
            if row is not None: near[arm] = [r for r in projected[arm] if r['conflicts'] <= row[0]][-12:]
    out.update(status='localized' if not difference['complete_equal'] else 'equal_through_stop',
        validated_overlap_rows=0, decisions=difference,
        matching_decisions_from_start=m['common_decisions'] + difference['matching_rows'],
        boundaries=bounds.compare(projected['candidate'], projected['kissat']),
        last_boundaries_before_difference=near, ignored_boundary_names=ignored)
with target.open('x') as stream: json.dump(out, stream, indent=2)
print(json.dumps({k: v for k, v in out.items() if k not in ('hashes', 'prefixes')}, indent=2))
