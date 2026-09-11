#!/usr/bin/env python3
"""Conserving cross-rule transition census on retained7144 and native samples."""
from collections import Counter, defaultdict
import importlib.util
import json
import lzma
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import Frame, Sample, samples, phase

CURRENT = tuple('tactix::' + name + '(' for name in (
    'kernel_rebuild_large_watches', 'kernel_drop_large_watches',
    'kernel_flush_large_watches', 'kernel_compact_dense_entry_watches',
    'kernel_deduplicate_binary_watches', 'kernel_dense_connect',
    'kernel_rebuild_dense_occurrences', 'rebuild_dense_large_occurrences'))
NATIVE = tuple(name for name in (
    'kissat_enter_dense_mode', 'kissat_resume_sparse_mode',
    'kissat_watch_large_clauses', 'kissat_flush_large_watches',
    'kissat_flush_large_connected', 'kissat_connect_irredundant_large_clauses'))
PROPAGATE = ('kissat_probing_propagate', 'kissat_search_propagate')


def census(stream, binary, selected, entries, caller=None, sites=None):
    totals = Counter()
    by_phase, by_entry, by_caller, leaves = (defaultdict(Counter) for _ in range(4))
    count = 0
    for sample in stream:
        assert sample.period > 0 and sample.frames
        count += 1
        name, _ = phase(sample, binary, entries, caller, sites)
        assert name != 'unresolved_phase_caller'
        matches = [i for i, f in enumerate(sample.frames)
                   if f.dso == binary and f.symbol.startswith(selected)]
        category = 'outside_selected_transitions'
        if matches:
            index = matches[-1]  # outermost: nested helpers are counted once
            category = 'selected_transition'
            if any(f.dso == binary and f.symbol.startswith(PROPAGATE)
                   for f in sample.frames[:index]):
                category = 'native_resume_propagation'
            entry = sample.frames[index].symbol
            parent = sample.frames[index + 1] if index + 1 < len(sample.frames) else None
            parent_name = parent.symbol + ' [' + parent.dso + ']' if parent else '<missing caller>'
            leaf = sample.frames[0]
            by_entry[category][entry] += sample.period
            by_caller[category][parent_name] += sample.period
            leaves[category][leaf.symbol + ' [' + leaf.dso + ']'] += sample.period
        totals[category] += sample.period
        by_phase[name][category] += sample.period
    for category, value in totals.items():
        assert sum(row[category] for row in by_phase.values()) == value
        if category != 'outside_selected_transitions':
            for table in (by_entry, by_caller, leaves):
                assert sum(table[category].values()) == value
    return dict(samples=count, total_period=sum(totals.values()), totals=dict(totals),
                by_phase={k: dict(v) for k, v in by_phase.items()},
                by_entry={k: dict(v) for k, v in by_entry.items()},
                by_caller={k: dict(v) for k, v in by_caller.items()},
                leaves={k: dict(v) for k, v in leaves.items()})


def self_test():
    f = lambda name, dso='solver': Frame(1, name, dso)
    rows = [Sample(2, (f('outer'),)),
            Sample(3, (f('malloc', 'libc'), f('inner'), f('outer'), f('probe'))),
            Sample(5, (f('outer', 'foreign'),)),
            Sample(7, (f('kissat_search_propagate'), f('outer'))),
            Sample(11, (f('outer'), f('outer'))),
            Sample(13, (f('kissat_search_propagate', 'foreign'), f('outer')))]
    result = census(rows, 'solver', ('outer', 'inner'), {'probe': 'probe'})
    assert result['samples'] == 6 and result['total_period'] == 41
    assert result['totals'] == dict(selected_transition=29,
        outside_selected_transitions=5, native_resume_propagation=7)
    assert result['by_entry']['selected_transition'] == {'outer': 29}
    assert result['by_phase']['probe'] == {'selected_transition': 3}
    assert result['leaves']['selected_transition']['malloc [libc]'] == 3
    assert census([], 'solver', ('outer',), {})['total_period'] == 0
    for bad in ([Sample(0, (f('outer'),))], [Sample(1, ())],
                [Sample(1, (f('outer'), f('phase')))]):
        try:
            census(bad, 'solver', ('outer',), {'phase': None}, 'caller', {})
        except AssertionError:
            pass
        else:
            raise AssertionError('invalid sample/unknown phase accepted')
    print('Transition census: nesting, recursion, foreign DSO, external descendants, '
          'resume propagation, missing caller/phase, empty and invalid input PASS')


def main():
    self_test()
    assert sys.argv[1:] in ([], ['--self-test'])
    if sys.argv[1:]:
        return
    target = root / 'benchmarks/reports/2026-09-11-transition-cost-7246.json'
    assert not target.exists(), 'successful writer frozen'
    hashes = {}
    def bind(p, expected=None):
        digest = sha(p)
        assert expected is None or expected == digest, p
        hashes[str(p.relative_to(root))] = digest
    def read(p):
        bind(p)
        return json.loads(p.read_text())
    c = root / 'benchmarks/runs/20260910-7152-vivify-compact-profiles'
    manifest = read(c / 'manifest.json')
    results = {}
    for wave in range(3):
        extracted = root / f'benchmarks/runs/20260910-7153-vivify-compact-samples/wave{wave}'
        summary = read(extracted / 'summary.json')
        binary = c / manifest['arms']['candidate']['path']
        bind(binary, manifest['arms']['candidate']['sha256'])
        path = extracted / 'candidate.samples.txt.xz'
        bind(path, summary['hashes'][str(path.relative_to(root))])
        with lzma.open(path, 'rt') as stream:
            value = census(samples(stream), str(binary), CURRENT,
                {manifest['phase_entry']: None}, manifest['phase_caller'],
                {int(k): v for k, v in manifest['call_sites']['candidate']['sites'].items()})
        expected = summary['arms']['candidate']
        assert value['samples'] == expected['samples']
        assert value['total_period'] == expected['total_period']
        assert {k: sum(v.values()) for k, v in value['by_phase'].items()} == expected['phase_periods']
        results[f'retained7144-wave{wave}'] = value
        print(f'retained7144-wave{wave}', value['totals'], flush=True)
    path = root / 'benchmarks/runs/20260910-7081-rank35-callstack-cost/lifecycle-summary-7082.json'
    native = read(path)
    binary = root / 'benchmarks/runs/20260910-7002-largest-gap-release-profiles/pins/kissat'
    bind(binary, native['hashes'][str(binary.relative_to(root))])
    path = path.parent / 'kissat.samples.txt.xz'
    bind(path, native['hashes'][str(path.relative_to(root))])
    with lzma.open(path, 'rt') as stream:
        value = census(samples(stream), str(binary), NATIVE,
            {'kissat_probe_initially': 'initial_probe', 'kissat_probe': 'probe',
             'kissat_eliminate': 'eliminate'})
    expected = native['arms']['kissat']
    assert value['samples'] == expected['samples'] and value['total_period'] == expected['total_period']
    assert {k: sum(v.values()) for k, v in value['by_phase'].items()} == expected['phase_periods']
    results['native'] = value
    print('native', value['totals'], flush=True)
    for p in (Path(__file__), root / 'tools/bench/perf_callstack.py',
              root / '.claude/worktrees/runtime-vivify-compact-7144/src/kernel/kernel.cpp',
              root / '.claude/worktrees/runtime-vivify-compact-7144/src/rules/sweep/sweep.cpp',
              root / 'kissat/src/dense.c', root / 'kissat/src/watch.c'):
        bind(p)
    with target.open('x') as out:
        json.dump(dict(hashes=hashes, selected_prefixes=dict(current=CURRENT, native=NATIVE),
            native_propagation_prefixes=PROPAGATE, arms=results,
            limitations='A disjoint sample census, not a complete equivalent-operation denominator or removable-time estimate. Current separate occurrence images and native shared vectors have different responsibilities. Inlined operations outside selected ancestry are missing; absent phase frames remain explicit. Native sparse resume includes propagation, split separately only when its frame is observed. Three retained7144 profiles versus one historical native observation; no native rerun, exact invocation counts, new solver execution, performance acceptance or source change.'), out, indent=2)


if __name__ == '__main__':
    main()
