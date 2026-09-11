#!/usr/bin/env python3
"""Localize complete dense-index rebuilding in all frozen 7030 paired streams."""
from collections import Counter
import json
import lzma
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import Frame, Sample, samples, phase

PREFIX = 'tactix::rebuild_dense_large_occurrences('

def summarize(stream, binary, entries, caller, sites):
    total = selected = count = 0
    phases, parents, paths, leaves = Counter(), Counter(), Counter(), Counter()
    for sample in stream:
        assert sample.period > 0 and sample.frames
        total += sample.period
        count += 1
        matches = [i for i, f in enumerate(sample.frames)
                   if f.dso == binary and f.symbol.startswith(PREFIX)]
        if not matches:
            continue
        selected += sample.period
        index = matches[-1]
        name, _ = phase(sample, binary, entries, caller, sites)
        phases[name] += sample.period
        tail = sample.frames[index + 1:]
        parent = tail[0].symbol if tail else '<missing caller>'
        parents[name + ' | ' + parent] += sample.period
        paths[name + ' | ' + ' -> '.join(f.symbol for f in tail[:6])] += sample.period
        leaves[name + ' | ' + sample.frames[0].symbol] += sample.period
    assert all(sum(c.values()) == selected for c in (phases, parents, paths, leaves))
    return dict(total_period=total, samples=count, selected_period=selected,
                by_phase=dict(phases), by_parent=dict(parents),
                by_path=dict(paths), by_leaf=dict(leaves))

def self_test():
    binary = '/fixture'
    f = lambda s, d=binary: Frame(1, s, d)
    target = f(PREFIX + 'fixture)')
    records = [Sample(2, (f('malloc', 'libc'), target, f('collector'))),
               Sample(3, (target, target, f('reconnect'))),
               Sample(5, (f(PREFIX + 'fixture)', '/foreign'),)),
               Sample(7, (target,)), Sample(11, (f('unrelated'),))]
    out = summarize(records, binary, {}, None, {})
    assert out['total_period'] == 28 and out['selected_period'] == 12
    assert out['by_phase'] == {'no_phase_frame': 12}
    assert out['by_parent'] == {'no_phase_frame | collector': 2,
                              'no_phase_frame | reconnect': 3,
                              'no_phase_frame | <missing caller>': 7}
    for invalid in (Sample(0, (target,)), Sample(1, ())):
        try:
            summarize([invalid], binary, {}, None, {})
        except AssertionError:
            pass
        else:
            raise AssertionError('invalid sample accepted')
    print('Caller attribution: descendants, recursion, foreign DSO, missing caller, conservation and invalid samples PASS')

def main():
    self_test()
    assert sys.argv[1:] in ([], ['--self-test'])
    if sys.argv[1:]:
        return
    target = root / 'benchmarks/reports/2026-09-10-dense-rebuild-localization-7160.json'
    assert not target.exists(), 'frozen result must not be overwritten'
    hashes = {}
    def bind(p, expected=None):
        digest = sha(p)
        assert expected is None or digest == expected, p
        hashes[str(p.relative_to(root))] = digest
    campaign = root / 'benchmarks/runs/20260910-7152-vivify-compact-profiles'
    manifest_path = campaign / 'manifest.json'
    bind(manifest_path)
    manifest = json.loads(manifest_path.read_text())
    arm = 'previous'
    binary = str(campaign / manifest['arms'][arm]['path'])
    bind(Path(binary), manifest['arms'][arm]['sha256'])
    sites = {int(k): v for k, v in manifest['call_sites'][arm]['sites'].items()}
    results = []
    for wave in range(3):
        c = root / f'benchmarks/runs/20260910-7153-vivify-compact-samples/wave{wave}'
        summary_path = c / 'summary.json'
        bind(summary_path)
        summary = json.loads(summary_path.read_text())
        p = c / 'previous.samples.txt.xz'
        bind(p, summary['hashes'][str(p.relative_to(root))])
        with lzma.open(p, 'rt') as stream:
            result = summarize(samples(stream), binary, {manifest['phase_entry']: None},
                               manifest['phase_caller'], sites)
        assert result['samples'] == summary['arms'][arm]['samples']
        assert result['total_period'] == summary['arms'][arm]['total_period']
        assert result['selected_period'] > 0
        results.append(result)
        print(wave, json.dumps(result['by_parent']), flush=True)
    for rel in ('src/kernel/kernel.cpp', 'src/kernel/kernel.hpp',
                'src/rules/strengthen/strengthen.cpp', 'src/policies/default_policy.cpp'):
        bind(root / '.claude/worktrees/runtime-collect-retry-7030' / rel)
    for p in (Path(__file__), root / 'tools/bench/perf_callstack.py',
              root / 'kissat/src/eliminate.c'):
        bind(p)
    with target.open('x') as stream:
        json.dump(dict(hashes=hashes, waves=results,
            scope='Complete dense rebuild inclusive samples in all three frozen7030 control streams. Caller attribution is sampled ancestry, not invocation counts. No new solver execution or exact operation-cost claim.',
            decision='Distinguish mandatory reconnection from repeated materialization at collection before selecting a structural redesign. Do not remove the native-required round/forward boundaries.'), stream, indent=2)

if __name__ == '__main__':
    main()
