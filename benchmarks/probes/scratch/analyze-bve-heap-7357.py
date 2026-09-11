#!/usr/bin/env python3
"""Validate whole-run replay before inspecting selected BVE heap state."""
import gzip
from itertools import zip_longest
import json
import math
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

FIELDS = ('stage', 'trigger', 'subject', 'score', 'positive', 'negative', 'pos', 'root', 'size')

def parse_heap(text):
    lines = text.splitlines()
    assert lines and lines[0] == '# bve-heap-observer\t1'
    rows = []
    for line in lines[1:]:
        parts = line.split('\t')
        assert len(parts) == 10 and parts[0] == 'tactix'
        row = {}
        for key, part in zip(FIELDS, parts[1:]):
            name, value = part.split('=', 1)
            assert name == key and value
            row[key] = value
        assert row['stage'] in ('initial', 'pop', 'update-reinsert', 'update-add')
        assert re.fullmatch(r'-?0x[0-9a-f]+(?:\.[0-9a-f]+)?p[+-][0-9]+', row['score'])
        assert math.isfinite(float.fromhex(row['score']))
        for key in ('trigger', 'subject', 'positive', 'negative', 'pos', 'root', 'size'):
            assert re.fullmatch(r'-?(?:0|[1-9][0-9]*)', row[key])
            row[key] = int(row[key])
        assert row['subject'] == 22861
        assert all(row[key] >= 0 for key in ('trigger', 'positive', 'negative', 'root', 'size'))
        assert -1 <= row['pos'] < row['size'] or (row['pos'] == -1 and row['size'] == 0)
        assert (row['root'] == 0) == (row['size'] == 0)
        if row['stage'] == 'initial':
            assert not rows and row['trigger'] == 0
        else:
            assert rows and row['trigger'] > 0
        rows.append(row)
    assert rows and rows[0]['stage'] == 'initial'
    return rows

def fixtures():
    sample = ('# bve-heap-observer\t1\n'
              'tactix\tstage=initial\ttrigger=0\tsubject=22861\tscore=0x1.e0c35p+21'
              '\tpositive=21\tnegative=15\tpos=2\troot=68992\tsize=3\n')
    absent = ('tactix\tstage=pop\ttrigger=22861\tsubject=22861\tscore=0x1.e0c35p+21'
              '\tpositive=21\tnegative=15\tpos=-1\troot=68992\tsize=2\n')
    assert parse_heap(sample)[0]['score'] == '0x1.e0c35p+21'
    assert parse_heap(sample + absent)[-1]['pos'] == -1
    assert parse_heap(sample + absent.replace('root=68992\tsize=2', 'root=0\tsize=0'))[-1]['size'] == 0
    invalid = [sample.replace('subject=22861', 'subject=22862'),
               sample.replace('score=0x1.e0c35p+21', 'score=nan'),
               sample.replace('pos=2', 'pos=3'), sample.replace('pos=2', 'pos=-2'),
               sample.replace('positive=21', 'positive=-1'),
               sample.replace('negative=15', 'positive=15'),
               sample.replace('stage=initial', 'stage=unknown'),
               sample.replace('root=68992', 'root=0'),
               sample.replace('trigger=0', 'trigger=1'), sample + sample.splitlines()[1] + '\n',
               sample.replace('\tsize=3', '\tsize=3\textra=1')]
    for bad in invalid:
        try:
            parse_heap(bad)
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError('invalid selected heap schema accepted')
    print('heap parser: hexadecimal scores, absent membership, 11 negative controls PASS')

fixtures()
if sys.argv[1:] == ['--self-test']:
    sys.exit(0)
assert not sys.argv[1:]
c = root / 'benchmarks/runs/20260911-7356-bve-heap'
old = root / 'benchmarks/runs/20260911-7353-root-phase-trace'
target = root / 'benchmarks/reports/2026-09-11-bve-heap-analysis-7357.json'
assert not target.exists()
helper = Path(__file__).with_name('check-submit-bve-heap-7356.py')
checked = subprocess.run([sys.executable, helper], text=True, capture_output=True)
assert checked.returncode == 0, (checked.stdout, checked.stderr)
hashes, states, heaps = {}, [], []

def bind(path, expected=None):
    digest = sha(path)
    assert expected is None or digest == expected, path
    hashes[str(path.relative_to(root))] = digest

for task in range(2):
    path = c / f'results/{task:03d}.json'
    final = path.exists()
    if not final:
        path = c / f'provisional/{task:03d}.json'
    record = json.loads(path.read_text())
    state = classify(record, final)
    assert record['task'] == task and state['status'] in ('pending_validation', 'checked_UNSAT')
    states.append(state)
    bind(path)
    prior_path = old / f'results/{task:03d}.json'
    prior = json.loads(prior_path.read_text())
    assert classify(prior, True)['checked']
    bind(prior_path)
    for kind in ('decisions', 'macro', 'active', 'inprocess', 'heap'):
        path = c / f'traces/{task:03d}-{kind}.tsv.gz'
        bind(path)
        if final:
            bind(path, record['trace_artifacts'][kind]['sha256'])
        if kind != 'heap':
            oracle = prior['trace_artifacts'][kind]
            bind(old / oracle['path'], oracle['sha256'])
            bind(path, oracle['sha256'])
        else:
            with gzip.open(path, 'rt') as stream:
                heaps.append(parse_heap(stream.read()))

def first_difference(fields):
    for ordinal, (a, b) in enumerate(zip_longest(*heaps), 1):
        if a is None or b is None or any(a[key] != b[key] for key in fields):
            return dict(ordinal=ordinal, parent=a, candidate=b)
    return None

initial = [rows[0] for rows in heaps]
assert initial[0]['positive'] == initial[1]['positive'] == 21
assert initial[0]['negative'] == initial[1]['negative'] == 15
delta = float.fromhex(initial[1]['score']) - float.fromhex(initial[0]['score'])
for path in (Path(__file__), helper, c / 'manifest.json', c / 'launch.json',
             root / '.claude/worktrees/runtime-debug-contracts-7339/src/policies/default_policy.cpp',
             root / '.claude/worktrees/runtime-debug-contracts-7339/src/policies/heuristics.cpp'):
    bind(path)
summary = dict(status='WHOLE_REPLAY_AND_SELECTED_HEAP_VALIDATED', hashes=hashes, states=states,
    whole_7353_four_archive_replay=True, row_counts=list(map(len, heaps)),
    first_raw_difference=first_difference(FIELDS),
    first_position_or_membership_difference=first_difference(('pos',)),
    initial_candidate_minus_parent_score=delta,
    selected_rows=heaps, reader_output=checked.stdout,
    interpretation='At equal physical occurrence counts and focused mode, the negated additive BVE score differs by +29: candidate relevance/stamp is 29 smaller. Incoming numeric ordering already differs before BVE mutations. Candidate pops external22861; parent ends with it still present at position3. This selected-subject observer does not enumerate every heap pop or expose all incoming scores.',
    limitation='Not a complete causal proof of where stamps first changed or a native-parity judgment. No runtime gain, memory-saving, or source-promotion claim. Additional topology comparison aligns emitted rows, not every solver event.')
with target.open('x') as out:
    json.dump(summary, out, indent=2)
print(json.dumps({key: value for key, value in summary.items()
                  if key not in ('hashes', 'selected_rows', 'reader_output')}, indent=2))
