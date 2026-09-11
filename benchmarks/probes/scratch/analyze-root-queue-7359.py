#!/usr/bin/env python3
"""Require complete prior trace replay, then locate changed queue restamping."""
import gzip
from itertools import zip_longest
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

def parse_queue(lines):
    result = []
    seen = set()
    for line in lines:
        fields = line.rstrip('\n').split('\t')
        if fields[0] == 'snapshot':
            assert len(fields) == 9 and fields[1]
            values = list(map(int, fields[2:]))
            assert all(v >= 0 for v in values) and values[2] in (0, 1)
            result.append(dict(stage=fields[1], conflicts=values[0], decisions=values[1],
                stable=values[2], stamp=values[3], search=values[4],
                search_stamp=values[5], assigned=values[6], members=[]))
            seen = set()
        else:
            assert fields[0] == 'member' and len(fields) == 3 and result
            external, stamp = map(int, fields[1:])
            assert external > 0 and stamp >= 0 and external not in seen
            seen.add(external)
            result[-1]['members'].append([external, stamp])
    assert result
    return result

def fixtures():
    good = 'snapshot\treduce-compact-pre\t14492\t1037401\t0\t100\t2\t4\t30\nmember\t2\t4\n'
    assert parse_queue(good.splitlines())[0]['members'] == [[2, 4]]
    bad = [good + 'member\t2\t5\n', good.replace('member\t2', 'member\t0'),
           good.replace('\t0\t100', '\t2\t100'), good.replace('\t2\t4\n', '\t2\t-1\n'),
           good.replace('member\t2\t4', 'member\t2\t4\textra'), 'member\t2\t4\n']
    for text in bad:
        try: parse_queue(text.splitlines())
        except (AssertionError, ValueError): pass
        else: raise AssertionError('invalid queue snapshot accepted')
    print('queue parser: ordered identities, six negative controls PASS')

fixtures()
if sys.argv[1:] == ['--self-test']: sys.exit(0)
assert not sys.argv[1:]
c = root / 'benchmarks/runs/20260911-7358-root-queue'
old = root / 'benchmarks/runs/20260911-7356-bve-heap'
target = root / 'benchmarks/reports/2026-09-11-root-queue-analysis-7359.json'
assert not target.exists()
helper = Path(__file__).with_name('check-submit-root-queue-7358.py')
check = subprocess.run([sys.executable, helper], text=True, capture_output=True)
assert check.returncode == 0, (check.stdout, check.stderr)
hashes, states, snapshots = {}, [], []
def bind(path, expected=None):
    digest = sha(path)
    assert expected is None or digest == expected, path
    hashes[str(path.relative_to(root))] = digest
for task in range(2):
    path = c / f'results/{task:03d}.json'
    final = path.exists()
    if not final: path = c / f'provisional/{task:03d}.json'
    r = json.loads(path.read_text())
    state = classify(r, final)
    assert r['task'] == task and state['status'] in ('pending_validation', 'checked_UNSAT')
    states.append(state); bind(path)
    prior_path = old / f'results/{task:03d}.json'
    prior = json.loads(prior_path.read_text())
    assert classify(prior, True)['checked']; bind(prior_path)
    for kind in ('decisions', 'macro', 'active', 'inprocess', 'heap', 'queue'):
        path = c / f'traces/{task:03d}-{kind}.tsv.gz'; bind(path)
        if final: bind(path, r['trace_artifacts'][kind]['sha256'])
        if kind != 'queue':
            oracle = prior['trace_artifacts'][kind]
            bind(old / oracle['path'], oracle['sha256']); bind(path, oracle['sha256'])
        else:
            with gzip.open(path, 'rt') as stream: snapshots.append(parse_queue(stream))
selected = []
for arm in snapshots:
    rows = [row for row in arm if row['conflicts'] == 14492 and row['stage'] in ('reduce-compact-pre', 'reduce-compact-post')]
    assert [row['stage'] for row in rows] == ['reduce-compact-pre', 'reduce-compact-post']
    selected.append(rows)
pre_equal = selected[0][0] == selected[1][0]
members = [row[1]['members'] for row in selected]
ids = [set(v for v, stamp in group) for group in members]
parent_only, candidate_only = sorted(ids[0] - ids[1]), sorted(ids[1] - ids[0])
filtered_parent = [pair[0] for pair in members[0] if pair[0] in ids[1]]
same_retained_order = filtered_parent == [pair[0] for pair in members[1]]
stamp_maps = [dict(group) for group in members]
delta_counts = {}
for v in ids[0] & ids[1]:
    delta = stamp_maps[1][v] - stamp_maps[0][v]
    delta_counts[delta] = delta_counts.get(delta, 0) + 1
first = None
for ordinal, (a, b) in enumerate(zip_longest(*snapshots), 1):
    if a != b:
        first = dict(ordinal=ordinal, parent={key: value for key, value in a.items() if key != 'members'} if a else None,
                     candidate={key: value for key, value in b.items() if key != 'members'} if b else None)
        break
for path in (Path(__file__), helper, c / 'manifest.json', c / 'launch.json'): bind(path)
summary = dict(status='WHOLE_REPLAY_AND_QUEUE_SNAPSHOTS_VALIDATED', hashes=hashes,
    states=states, whole_7356_five_archive_replay=True, snapshot_counts=list(map(len, snapshots)),
    first_raw_snapshot_difference=first, pre_snapshot_exact=pre_equal,
    selected_headers=[[{k: v for k, v in row.items() if k != 'members'} for row in arm] for arm in selected],
    selected_member_counts=[[len(row['members']) for row in arm] for arm in selected],
    post_parent_only=parent_only, post_candidate_only=candidate_only,
    post_retained_order_exact=same_retained_order, post_retained_stamp_delta_counts=delta_counts,
    selected_22861_pre_stamps=[dict(arm[0]['members']).get(22861) for arm in selected],
    selected_22861_post_stamps=[dict(arm[1]['members']).get(22861) for arm in selected],
    reader_output=check.stdout,
    limitation='Queue snapshots identify ordered members and stamps, not root values or the first missed deactivation. No native run, unit-cost timing, memory-saving or promotion claim. Complete source archives retained and hash-bound; no raw exclusions used to claim whole-state parity.')
with target.open('x') as out: json.dump(summary, out, indent=2)
print(json.dumps({key: value for key, value in summary.items() if key not in ('hashes', 'reader_output')}, indent=2))
