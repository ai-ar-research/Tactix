#!/usr/bin/env python3
"""Bind complete observer replay, then account selected Reorder input weights."""
from collections import Counter
import importlib.util
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from binary_snapshot import read_snapshots
from reorder_input import read_active, root_boundary, reconstruct

c = root / 'benchmarks/runs/20260909-6629-rank35-reorder-input'
target = c / 'summary-6631.json'
assert not target.exists(), 'Successful writer is frozen'
arms = ('candidate', 'previous')
if any(not (c / f'results/{arm}.json').exists() for arm in arms):
    print('Input diagnostic still pending; no summary written')
    sys.exit(0)
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(path, expected=None):
    digest = sha(path)
    if expected is not None:
        assert digest == expected, path
    hashes[str(path.relative_to(root))] = digest
    return digest
for rel, digest in m['hashes'].items():
    bind(c / rel, digest)
for rel, digest in m['dependencies'].items():
    bind(root / rel, digest)
for name in ('manifest.json', 'launch.json'):
    bind(c / name)
parser_path = Path(__file__).with_name('compare-rank32-formula-5785.py')
spec = importlib.util.spec_from_file_location('formula5785', parser_path)
formula_parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formula_parser)
formula_parser.fixtures()
for path in (Path(__file__), parser_path, root / 'tools/bench/reorder_input.py',
             root / 'tools/bench/test_reorder_input.py', root / 'tools/bench/binary_snapshot.py'):
    bind(path)
parent = root / m['cached_controls']
prior = json.loads((parent / 'summary-6626.json').read_text())
data = {}; formulas = {}; binary_rows = {}; active_sets = {}; boundaries = {}
for arm in arms:
    p = c / f'results/{arm}.json'; bind(p)
    r = json.loads(p.read_text())
    old = json.loads((parent / f'results/{arm}.json').read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
    assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
    assert r['partial_certificate'] == old['partial_certificate']
    assert set(r['trace_sha256']) - set(old['trace_sha256']) == {f'{arm}.{name}.tsv' for name in ('formula', 'binaries', 'active')}
    assert set(old['trace_sha256']) <= set(r['trace_sha256'])
    for name, digest in r['trace_sha256'].items():
        bind(c / 'traces' / name, digest)
    for name, digest in old['trace_sha256'].items():
        assert r['trace_sha256'][name] == digest, (arm, name, 'Replay changed: stop interpretation')
    boundary = root_boundary((c / f'traces/{arm}.macro.tsv').read_text(), event='reorder_begin', conflict=3074062)
    assert boundary['decisions'] == '161620843' and boundary['mode'] == 'stable'
    boundaries[arm] = boundary
    content = (c / f'traces/{arm}.formula.tsv').read_text()
    assert content.endswith('\n'), 'Truncated formula row'
    formulas[arm] = formula_parser.read_rows(c / f'traces/{arm}.formula.tsv')
    with (c / f'traces/{arm}.binaries.tsv').open() as stream:
        snapshots = read_snapshots(stream, 'tactix', 3074062, [('reorder_begin', '-')])
    assert len(snapshots) == 1
    binary_rows[arm] = snapshots[0]['rows']
    active_sets[arm] = read_active((c / f'traces/{arm}.active.tsv').read_text(), event='reorder_begin', conflict=3074062)
    assert len(active_sets[arm]) == int(boundary['unassigned'])
    data[arm] = reconstruct(1373, formulas[arm], binary_rows[arm], active_sets[arm], level=int(boundary['level']))
    selected = prior['selected_reorder'][arm]
    assert len(selected) == 2 and selected[0]['score'] == 0
    assert data[arm]['combined'] == selected[1]['score'], (arm, data[arm], selected)
def delta(left, right):
    counts = Counter(left) - Counter(right)
    return [dict(row=row, copies=count) for row, count in sorted(counts.items())]
out = dict(rank=35, variable=1373, complete_own_replay=True, score_reconstruction_exact=True,
    selected_boundary=boundaries, weights=data,
    large_row_counts={arm: len(rows) for arm, rows in formulas.items()},
    full_large_rows_equal=formulas['candidate'] == formulas['previous'],
    candidate_only_large=delta(formulas['candidate'], formulas['previous']),
    previous_only_large=delta(formulas['previous'], formulas['candidate']),
    candidate_only_binary=delta(binary_rows['candidate'], binary_rows['previous']),
    previous_only_binary=delta(binary_rows['previous'], binary_rows['candidate']),
    active_counts={arm: len(rows) for arm, rows in active_sets.items()},
    candidate_only_active=sorted(active_sets['candidate'] - active_sets['previous']),
    previous_only_active=sorted(active_sets['previous'] - active_sets['candidate']), hashes=hashes,
    limitation='Exact selected input accounting after complete own trace/partial-proof replay. Active membership proves free only at independently verified level0. Missing endpoint values are rejected. This does not prove the first input mutation, whole-formula equivalence, native parity, heap causality, final proof acceptance or runtime benefit. No solver change.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps({key: value for key, value in out.items() if key not in ('hashes', 'weights')}, indent=2))
for arm, result in data.items():
    print(arm, json.dumps({key: value for key, value in result.items() if key not in ('large', 'binaries')}))
