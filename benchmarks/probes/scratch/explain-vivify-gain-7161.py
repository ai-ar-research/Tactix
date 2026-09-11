#!/usr/bin/env python3
"""Quantify schedule savings and whole-solver dilution from every paired wave."""
import json
import lzma
from pathlib import Path
import statistics
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import samples
from perf_function_envelope import function_envelope

target = root / 'benchmarks/reports/2026-09-10-vivify-gain-explanation-7161.json'
assert not target.exists()
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
campaign = root / 'benchmarks/runs/20260910-7152-vivify-compact-profiles'
manifest_path = campaign / 'manifest.json'
bind(manifest_path)
m = json.loads(manifest_path.read_text())
waves = []
for wave in range(3):
    c = root / f'benchmarks/runs/20260910-7153-vivify-compact-samples/wave{wave}'
    p = c / 'summary.json'
    bind(p)
    data = json.loads(p.read_text())
    arms = {}
    for arm in ('candidate', 'previous'):
        p = c / f'{arm}.samples.txt.xz'
        bind(p, data['hashes'][str(p.relative_to(root))])
        binary = campaign / m['arms'][arm]['path']
        bind(binary, m['arms'][arm]['sha256'])
        with lzma.open(p, 'rt') as stream:
            out = function_envelope(samples(stream), str(binary), 'tactix::vivify_schedule(',
                {m['phase_entry']: None}, m['phase_caller'],
                {int(k): v for k, v in m['call_sites'][arm]['sites'].items()})
        vivify = data['function_envelopes'][arm]
        assert out['samples'] == vivify['samples']
        assert out['total_period'] == vivify['total_period']
        assert 0 < out['selected_period'] <= vivify['selected_period']
        leaves = vivify['by_leaf']
        groups = {}
        for name, predicate in (
            ('probing_leaf', lambda s: 'propagate_literal<false, false, true>' in s),
            ('watch_rebuild_leaf', lambda s: s.startswith('tactix::kernel_rebuild_large_watches(')),
            ('watch_compaction_leaf', lambda s: s.startswith('tactix::kernel_compact_dense_entry_watches(')),
            ('watch_selection_leaf', lambda s: s.startswith('tactix::move_best_watch_to_front('))):
            groups[name] = sum(value for key, value in leaves.items() if predicate(key))
        arms[arm] = dict(schedule=out, vivify=vivify['selected_period'],
                         total=out['total_period'], disjoint_self_groups=groups)
        print(wave, arm, 'schedule', out['selected_period'], flush=True)
    a, b = arms['candidate'], arms['previous']
    saved = b['vivify'] - a['vivify']
    waves.append(dict(arms=arms, schedule_ratio=a['schedule']['selected_period']/b['schedule']['selected_period'],
        vivify_ratio=a['vivify']/b['vivify'], baseline_vivify_fraction=b['vivify']/b['total'],
        baseline_schedule_fraction_of_vivify=b['schedule']['selected_period']/b['vivify'],
        predicted_total_gain_if_other_cost_unchanged=saved/b['total'],
        observed_profile_total_gain=1-a['total']/b['total'],
        schedule_saved=b['schedule']['selected_period']-a['schedule']['selected_period'],
        vivify_saved=saved))
summary = {key: statistics.median(w[key] for w in waves) for key in (
    'schedule_ratio', 'vivify_ratio', 'baseline_vivify_fraction',
    'baseline_schedule_fraction_of_vivify', 'predicted_total_gain_if_other_cost_unchanged',
    'observed_profile_total_gain', 'schedule_saved', 'vivify_saved')}
for p in (Path(__file__), root / 'tools/bench/perf_function_envelope.py',
          root / 'tools/bench/test_perf_function_envelope.py',
          root / 'tools/bench/perf_callstack.py',
          root / '.claude/worktrees/runtime-vivify-compact-7144/src/rules/vivify/vivify.cpp',
          root / 'benchmarks/reports/2026-09-10-vivify-schedule-localization-7140.md'):
    bind(p)
with target.open('x') as stream:
    json.dump(dict(hashes=hashes, waves=waves, medians=summary,
        caveat='Sampled inclusive schedule envelope, all descendants included; complete Vivify contains it, so do not add them. Leaf groups are disjoint self costs only. Amdahl estimate assumes unchanged remainder and cycles/time proportionality; unprofiled7155 owns runtime/RSS. Rank35 only, no full50 gain claim. The 15% predeclared materiality target was not an established forecast.'), stream, indent=2)
print(json.dumps(summary, indent=2))
