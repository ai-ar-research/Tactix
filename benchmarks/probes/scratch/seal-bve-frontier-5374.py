#!/usr/bin/env python3
"""Validate observer neutrality and retain the candidate/schedule frontier."""
import collections
import hashlib
import json
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
arms = ('tactix', 'kissat')

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(1048576), b''):
            h.update(b)
    return h.hexdigest()

def validate(c, parent):
    m = json.loads((c / 'manifest.json').read_text())
    for rel, expected in m['hashes'].items():
        assert sha(c / rel) == expected, rel
    submission = json.loads((c / 'submission.json').read_text())
    job = submission.get('job_id', submission.get('job'))
    for i, arm in enumerate(arms):
        log = (c / 'logs' / f'{job}_{i}.log').read_text()
        assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
        p = c / 'traces' / f'{arm}.decisions.tsv'
        with (parent / 'traces' / p.name).open('rb') as f:
            assert f.read(p.stat().st_size) == p.read_bytes()
    return {p.name: sha(p) for p in (c / 'traces').iterdir()}

def rows(p):
    return [dict(x.split('=', 1) for x in l.split('\t')[1:])
            for l in p.read_text().splitlines() if not l.startswith('#')]

def seal(c, result):
    p = c / 'summary.json'
    if p.exists():
        assert json.loads(p.read_text()) == result
    else:
        p.write_text(json.dumps(result, indent=2) + '\n')
    print(c.name, result['conclusion'])

c = runs / '20260907-5374-bve-frontier'
hashes = validate(c, runs / '20260907-5373-million-prefix')
data = {a: rows(c / 'traces' / f'{a}.bve.tsv') for a in arms}
normalized = {a: [{k: r[k] for k in ('external', 'stage', 'conflicts', 'ticks', 'limit', 'applied')}
                  for r in rs if r['stage'] in ('begin', 'end')] for a, rs in data.items()}
t, k = normalized.values()
first = next(i for i, pair in enumerate(zip(t, k)) if pair[0] != pair[1])
assert first == 24 and t[first]['external'] == '11215' and k[first]['external'] == '10700'
ends = {a: [r for r in rs if r['external'] == '9959' and r['stage'] == 'end'] for a, rs in data.items()}
assert len(ends['tactix']) == len(ends['kissat']) == 1
assert ends['tactix'][0]['applied'] == '1' and ends['kissat'][0]['applied'] == '0'
state = {}
for a in arms:
    state[a] = [l.split('\t') for l in (c / 'traces' / f'{a}.state.tsv').read_text().splitlines()
                if l.split('\t')[1] in ('783731', '783732')]
    assert state[a][0][3:6] == ['-7041', '9959', '0']
seal(c, dict(stopped_clean=2, observer_prefix_identical=True,
             normalized_rows={a: len(rs) for a, rs in normalized.items()},
             matching_candidate_boundaries=first, first_candidate_difference={a: rs[first] for a, rs in normalized.items()},
             selected_9959_outcomes=ends, selected_choice_state=state, trace_sha256=hashes,
             conclusion='First 12 candidate attempts match; candidate 13 differs. Tactix later eliminates 9959 while Kissat retains it. Initial scheduling must be checked before blaming dynamic heap updates.'))

c = runs / '20260907-5375-bve-schedule'
hashes = validate(c, runs / '20260907-5374-bve-frontier')
members = {}
for a in arms:
    members[a] = {}
    for l in (c / 'traces' / f'{a}.schedule.tsv').read_text().splitlines():
        if not l.startswith('member\t'):
            continue
        r = dict(x.split('=') for x in l.split('\t')[1:])
        members[a][int(r['external'])] = {key: (struct.unpack('>d', bytes.fromhex(v))[0] if key == 'score' else int(v)) for key, v in r.items()}
t, k = members.values()
missing = sorted(k.keys() - t.keys())
assert missing == [6649,9355,10700,13407,19389,20877,21051,21323,21325,21551,21553,22022]
assert not t.keys() - k.keys()
assert all((t[i]['positive'], t[i]['negative']) == (k[i]['positive'], k[i]['negative']) for i in t)
offsets = collections.Counter(t[i]['score'] - k[i]['score'] for i in t)
assert offsets == {-1.0: 1244, 0.0: 1}
seal(c, dict(stopped_clean=2, observer_prefix_identical=True,
             initial_members={a: len(rs) for a, rs in members.items()}, kissat_only=missing,
             common_occurrence_counts_identical=True, common_score_offsets={str(v): n for v, n in offsets.items()},
             selected_members={a: {str(v): rs.get(v) for v in (22227,10700,11215,10988,20107,9959)} for a, rs in members.items()},
             trace_sha256=hashes,
             conclusion='Initial Tactix schedule omits 12 native candidates, including first differing candidate 10700. Shared occurrence counts agree. Inspect retry eligibility history; do not patch dynamic heap update order from this evidence.'))

c = runs / '20260907-5376-bve-history'
hashes = validate(c, runs / '20260907-5375-bve-schedule')
history = {a: [{key: r[key] for key in ('external', 'stage', 'conflicts', 'ticks', 'limit', 'applied')}
               for r in rows(c / 'traces' / f'{a}.bve.tsv') if r['stage'] in ('begin', 'end')] for a in arms}
assert history['tactix'] == history['kissat'] and len(history['tactix']) == 14
assert history['tactix'][-1]['conflicts'] == '240040'
seal(c, dict(stopped_clean=2, observer_prefix_identical=True, common_history=history['tactix'], trace_sha256=hashes,
             conclusion='All seven selected attempts through the second BVE pass at 240040 agree, including exact work ticks. Eligibility separates after that attempt and before scheduling at 343785; trace retry-mark producers in this interval.'))
