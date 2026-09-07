#!/usr/bin/env python3
"""Check selected bytes and unfiltered hashes, then expose the first choice state."""
import hashlib
import itertools
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5400-window-validation'
arms = ('candidate', 'kissat')
if any(not (c / 'results' / (a + '.json')).exists() for a in arms):
    print('5400 pending'); raise SystemExit(0)
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            h.update(block)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
old = root / m['parent_campaign']
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest, rel
validation = {}
for arm in arms:
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    for name, digest in r['trace_sha256'].items():
        assert sha(c / 'traces' / name) == digest
    count = selected = 0
    digest = 1469598103934665603
    common_hash = None
    with (old / 'traces' / (arm + '.decisions.tsv')).open('rb') as before, \
         (c / 'traces' / (arm + '.decisions.tsv')).open('rb') as after:
        for line in before:
            row = line.split(b'\t')
            conflict = int(row[0])
            if conflict > m['stop_conflict']:
                break
            count += 1
            digest = ((digest ^ (int(row[3]) & ((1 << 64) - 1))) * 1099511628211) & ((1 << 64) - 1)
            if count == m['common_decisions']:
                common_hash = digest
            if conflict >= m['stream_from']:
                assert after.readline() == line, (arm, count)
                selected += 1
        assert not after.read(1), arm
    assert selected > 0 and common_hash is not None
    row = next(l for l in (c / 'traces' / (arm + '.prefix.tsv')).read_text().splitlines()
               if not l.startswith('#')).split('\t')
    assert int(row[1]) == m['stop_conflict'] and int(row[4]) == count
    assert int(row[5], 16) == digest and int(row[8], 16) == common_hash
    before = (old / 'traces' / (arm + '.boundaries.tsv')).read_bytes().splitlines(keepends=True)
    expected = b''.join(l for l in before if l.startswith(b'#') or int(l.split(b'\t')[4]) <= m['stop_conflict'])
    assert (c / 'traces' / (arm + '.boundaries.tsv')).read_bytes() == expected
    choices = [l.split('\t') for l in (c / 'traces' / (arm + '.choice.tsv')).read_text().splitlines()]
    assert choices and all(int(r[0]) == m['choice_conflict'] for r in choices)
    validation[arm] = dict(full_prefix_rows=count, selected_rows=selected,
                           full_hash=f'{digest:016x}', common_hash=f'{common_hash:016x}',
                           boundary_prefix_identical=True, choices=choices)
a, b = validation['candidate']['choices'], validation['kissat']['choices']
first = next((dict(index=i, candidate=x, kissat=y) for i, (x, y) in enumerate(itertools.zip_longest(a, b))
              if x != y), None)
result = dict(clean_stops=2, validated_observer_filter=True, arms=validation,
              first_choice_state=first,
              trace_sha256={p.name: sha(p) for p in (c / 'traces').iterdir()},
              limitation='Choice state identifies the next source investigation, not a correctness or timing verdict. Legacy wrapped start_decisions ignored.')
p = c / 'summary.json'
if p.exists():
    assert json.loads(p.read_text()) == result
else:
    p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(dict(first_choice_state=first,
                     validation={a: {k: v for k, v in r.items() if k != 'choices'} for a, r in validation.items()}), indent=2))
