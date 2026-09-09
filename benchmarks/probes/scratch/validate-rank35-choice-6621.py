#!/usr/bin/env python3
"""Require complete current/previous replay before interpreting branch state."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from branch_state import read_heap, compare_heaps, read_choices
from decision_prefix import first_stream_difference

c = root / 'benchmarks/runs/20260909-6619-material-rank35-choice'
target = c / 'summary-6621.json'
assert not target.exists()
arms = ('candidate', 'previous')
if any(not (c / f'results/{arm}.json').exists() for arm in arms):
    print('Both owned results are not yet published; no summary written')
    sys.exit(0)
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(p, digest=None):
    actual = sha(p)
    if digest is not None:
        assert actual == digest, p
    hashes[str(p.relative_to(root))] = actual
    return actual
for rel, digest in m['hashes'].items():
    bind(c / rel, digest)
for rel, digest in m['dependencies'].items():
    bind(root / rel, digest)
for name in ('manifest.json', 'launch.json'):
    bind(c / name)
parent = root / m['cached_controls']
replays = {}
for arm in arms:
    p = c / f'results/{arm}.json'
    r = json.loads(p.read_text()); bind(p)
    old = json.loads((parent / f'results/{arm}.json').read_text())
    assert r['arm'] == arm and r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == sha(c / 'pins' / arm)
    assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.heap.tsv', arm + '.choice.tsv'}
    for name, digest in r['trace_sha256'].items():
        bind(c / 'traces' / name, digest)
    replays[arm] = dict(partial_proof_equal=r['partial_certificate'] == old['partial_certificate'],
        ordinary_trace_equal={name: r['trace_sha256'].get(name) == digest for name, digest in old['trace_sha256'].items()})
replayed = all(r['partial_proof_equal'] and all(r['ordinary_trace_equal'].values()) for r in replays.values())
out = dict(rank=35, complete_own_replay=replayed, replay=replays)
if not replayed:
    out['decision'] = 'Current or observed trajectory changed: localize changed replay before interpreting branch snapshots.'
    out['decisions'] = first_stream_difference(c / 'traces/candidate.decisions.tsv', parent / 'traces/candidate.decisions.tsv')
else:
    heaps = {}; choices = {}; selected_decisions = {}
    for arm in arms:
        with (c / f'traces/{arm}.heap.tsv').open() as stream:
            heaps[arm] = read_heap(stream)
        with (c / f'traces/{arm}.choice.tsv').open() as stream:
            choices[arm] = read_choices(stream, conflict=m['selected_conflict'], wanted=m['wanted_variable'])
        assert heaps[arm] and all(r['stable'] == 1 for r in choices[arm])
        # Extract every decision at the selected conflict from the already bound
        # complete stream, including rows beyond the first split in each arm.
        text = subprocess.check_output(['awk', '-F', '\t',
            '$1 == 3074183 {print} $1 > 3074183 {exit}', str(c / f'traces/{arm}.decisions.tsv')], text=True)
        selected_decisions[arm] = [list(map(int, line.split('\t'))) for line in text.splitlines()]
        projection = [[r['conflict'], r['decision'], r['level'], r['chosen'], 0] for r in choices[arm]]
        assert projection == selected_decisions[arm], 'Choice rows do not cover the entire selected decision window'
    differences = compare_heaps(heaps['candidate'], heaps['previous'])
    before = {}; split = {}
    for arm in arms:
        before[arm] = [r for r in choices[arm] if r['decision'] == m['last_common_decision']]
        split[arm] = [r for r in choices[arm] if r['decision'] == m['last_common_decision'] + 1]
        assert len(before[arm]) == len(split[arm]) == 1
    assert split['candidate'][0]['chosen'] == -482577 and split['previous'][0]['chosen'] == -486307
    out.update(heap_sizes={a: len(h) for a, h in heaps.items()}, heap_differences=differences,
        change_counts={k: len(v) for k, v in differences.items()},
        selected_heap_entries={a: {str(v): h.get(v) for v in (482577, 486307)} for a, h in heaps.items()},
        last_common_choice=before, first_split_choice=split,
        choice_row_counts={a: len(rows) for a, rows in choices.items()})
out['hashes'] = hashes
out['limitation'] = 'Two Tactix arms, not native equivalence. Complete per-arm ordinary hash and partial-proof replay is a neutrality/applicability check, not final proof acceptance. Heap snapshot follows last common assignment before subsequent propagation; assignment differences at the split may be consequences of earlier formula/propagation differences. Heap format has no declared row count; framing validation alone cannot detect deletion of a whole final row, so bind actual closed observer output to runner hashes.'
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps({k: v for k, v in out.items() if k not in ('hashes', 'heap_differences')}, indent=2))
