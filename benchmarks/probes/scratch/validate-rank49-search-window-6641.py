#!/usr/bin/env python3
"""Validate complete own replay and retain source-backed observer additions."""
from collections import Counter
import importlib.util
import itertools
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from search_window import read_search, macro_replay
c = root / 'benchmarks/runs/20260909-6639-rank49-search-window'
target = c / 'summary-6641.json'
assert not target.exists()
arms = ('candidate', 'kissat')
if any(not (c / f'results/{arm}.json').exists() for arm in arms):
    print('Search windows pending; no summary written'); sys.exit(0)
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
for rel, digest in m['hashes'].items():
    bind(c / rel, digest)
for rel, digest in m['dependencies'].items():
    bind(root / rel, digest)
for name in ('manifest.json', 'launch.json'):
    bind(c / name)
normalizer = Path(__file__).with_name('validate-square-phase-5486.py')
spec = importlib.util.spec_from_file_location('native_normalizer', normalizer)
native = importlib.util.module_from_spec(spec); spec.loader.exec_module(native)
native.self_test()
rows = {}; replay = {}
for arm in arms:
    p = c / f'results/{arm}.json'; bind(p)
    r = json.loads(p.read_text()); parent = root / m['cached_controls'][arm]
    old = json.loads((parent / f'results/{arm}.json').read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
    assert r['binary_sha256'] == old['binary_sha256'] == m['hashes']['pins/' + arm]
    assert r['partial_certificate'] == old['partial_certificate']
    assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.search.tsv'}
    assert set(old['trace_sha256']) <= set(r['trace_sha256'])
    for name, digest in r['trace_sha256'].items():
        bind(c / 'traces' / name, digest)
    info = dict(partial_proof_equal=True, exact_traces=[], additions={})
    for name, digest in old['trace_sha256'].items():
        if r['trace_sha256'][name] == digest:
            info['exact_traces'].append(name)
        elif name == 'candidate.macro.tsv':
            extra = macro_replay((parent / 'traces' / name).read_text(), (c / 'traces' / name).read_text())
            assert extra['extra_counts'] == dict(trace_begin=1, retier=15, checkpoint=90)
            info['additions'][name] = extra
        elif name == 'kissat.boundaries.tsv':
            def boundary_rows(path):
                return [line.split('\t') for line in path.read_text().splitlines() if not line.startswith('#')]
            count = native.validate_native_boundaries(boundary_rows(parent / 'traces' / name),
                boundary_rows(c / 'traces' / name), tuple(m['search_window']))
            info['additions'][name] = dict(extra_analysis_end=count)
        else:
            raise AssertionError((name, 'unexplained observer replay change'))
    for name in (arm + '.decisions.tsv', arm + '.prefix.tsv'):
        assert name in info['exact_traces']
    rows[arm] = read_search((c / f'traces/{arm}.search.tsv').read_text(),
        solver='tactix' if arm == 'candidate' else 'kissat', window=tuple(m['search_window']), cap=m['trace_row_cap'])
    replay[arm] = info
selected = {arm: [r for r in rr if m['search_window'][0] <= r['conflicts'] <= m['search_window'][1]] for arm, rr in rows.items()}
assert all(any(r['event'] == 'conflict' for r in rr) for rr in selected.values())
comparison = {}
fields = ('conflicts', 'decisions', 'level', 'payload')
for event in ('decision', 'imply', 'conflict', 'analysis_begin', 'learned', 'analysis_end'):
    streams = {arm: [r for r in rr if r['event'] == event] for arm, rr in selected.items()}
    key = lambda r: tuple(r[f] for f in fields) if r is not None else None
    first = next((dict(index=i, candidate=a, kissat=b) for i, (a,b) in enumerate(itertools.zip_longest(streams['candidate'], streams['kissat'])) if key(a) != key(b)), None)
    comparison[event] = dict(counts={arm:len(rr) for arm,rr in streams.items()}, first_raw_difference=first)
for p in (Path(__file__), root / 'tools/bench/search_window.py', root / 'tools/bench/test_search_window.py', normalizer):
    bind(p)
out = dict(rank=49, stop=100000, observer_replay_validated=True, replay=replay,
    total_rows={arm:len(rr) for arm,rr in rows.items()},
    selected_event_counts={arm:dict(Counter(r['event'] for r in rr)) for arm,rr in selected.items()},
    raw_event_comparison=comparison,
    conflicts={arm:[r for r in rr if r['event']=='conflict'] for arm,rr in selected.items()},
    learned={arm:[r for r in rr if r['event'].startswith('learned')] for arm,rr in selected.items()}, hashes=hashes,
    observer_premise='Manifest expected current macro byte identity; global tracing instead adds106 source-backed macro rows: trace_begin1,retier15,checkpoint90. The new tested subsequence validator proves every22699 original row unchanged except sequence shifts; extras retained in full. Native allows only analysis/end within the declared window. No solver failure or relaxed certificate oracle.',
    limitation='Raw selected event comparison is localization only. Payload order/metadata differences and native/Tactix assignment conventions need source interpretation; no full formula, root cause or runtime benefit inferred. Partial proof not final acceptance.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('hashes','replay','learned','conflicts')}, indent=2))
