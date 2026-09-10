#!/usr/bin/env python3
"""Validate intentional schedule stops and every physical member before scoring."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from bve_schedule import parse
from decision_prefix import first_stream_difference
c = root / 'benchmarks/runs/20260910-7018-rank35-bve-frontier'
target = c / 'schedule-summary-7020.json'
assert not target.exists()
arms = ('candidate_schedule', 'kissat_schedule')
if any(not (c / 'results' / (a + '.json')).exists() for a in arms):
    print('Schedule tasks pending; no summary written'); sys.exit(0)
m = json.loads((c / 'manifest.json').read_text()); parent = root / m['parent']
hashes = {}
def bind(p, expected=None):
    actual = sha(p)
    assert expected is None or actual == expected, p
    hashes[str(p.relative_to(root))] = actual
for rel, digest in m['hashes'].items(): bind(c / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
for p in (c / 'manifest.json', c / 'launch.json', Path(__file__)):
    bind(p)
headers, members, prefixes = {}, {}, {}
for arm in arms:
    base = arm.split('_')[0]
    p = c / 'results' / (arm + '.json'); bind(p)
    r = json.loads(p.read_text())
    assert r['arm'] == arm and r['exit_code'] == 0 and r['scratch_deleted']
    assert not Path(r['scratch']).exists() and r['partial_certificate']['bytes'] > 0
    assert r['binary_sha256'] == m['hashes']['pins/' + arm]
    expected = {arm + '.' + suffix + '.tsv' for suffix in ('decisions', 'boundaries', 'bve-schedule')}
    if base == 'candidate': expected.add(arm + '.macro.tsv')
    assert set(r['trace_sha256']) == expected
    for name, digest in r['trace_sha256'].items(): bind(c / 'traces' / name, digest)
    with (c / 'traces' / (arm + '.bve-schedule.tsv')).open() as stream:
        header, mm = parse(stream)
    assert int(header['conflicts']) == m['selected_conflict']
    assert int(header['decisions']) == 79045886
    headers[base], members[base] = header, mm
    stream = c / 'traces' / (arm + '.decisions.tsv')
    comparison = first_stream_difference(stream, parent / 'traces' / (base + '.decisions.tsv'))
    count = int(subprocess.check_output(['wc', '-l', str(stream)], text=True).split()[0])
    assert comparison['left'] is None and comparison['right'] is not None
    assert comparison['matching_rows'] == count and count > 0
    last = list(map(int, subprocess.check_output(['tail', '-n', '1', str(stream)], text=True).split()))
    assert last[0] <= m['selected_conflict'] and last[1] == int(header['decisions'])
    prefixes[base] = dict(rows=count, last=last, comparison=comparison)
left = {r['external']: r for r in members['candidate']}
right = {r['external']: r for r in members['kissat']}
shared = left.keys() & right.keys()
differences = [dict(external=key, candidate=left[key], kissat=right[key])
               for key in sorted(shared, key=int) if left[key] != right[key]]
out = dict(hashes=hashes, headers=headers, prefixes=prefixes,
    member_counts={arm: len(mm) for arm, mm in members.items()},
    candidate_only=sorted(left.keys()-right.keys(), key=int), kissat_only=sorted(right.keys()-left.keys(), key=int),
    member_differences=differences,
    limitation='Every emitted decision row replays own7011 prefix through intentional first-schedule stop. Partial proofs hashed/deleted, not checked; matching schedule scalars or members do not prove complete formula identity.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps({key: out[key] for key in ('headers', 'member_counts', 'candidate_only', 'kissat_only', 'member_differences')}, indent=2))
