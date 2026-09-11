#!/usr/bin/env python3
"""Validate full-function attribution against a frozen paired sample stream."""
import importlib.util
import json
import lzma
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import samples
from perf_function_envelope import function_envelope
target = root / 'benchmarks/reports/2026-09-10-vivify-envelope-replay-7154.json'
assert not target.exists()
c = root / 'benchmarks/runs/20260910-7132-bve-lifecycle-samples/wave0'
prior_path = c / 'summary.json'
prior = json.loads(prior_path.read_text())
hashes = {str(prior_path.relative_to(root)): sha(prior_path)}
for rel, digest in prior['hashes'].items():
    assert sha(root / rel) == digest, rel
    hashes[rel] = digest
profile = root / 'benchmarks/runs/20260910-7128-bve-lifecycle-profiles'
p = profile / 'manifest.json'; m = json.loads(p.read_text())
hashes[str(p.relative_to(root))] = sha(p)
arms = {}
for arm in ('candidate', 'previous'):
    stream_path = root / prior['records'][arm]['sample_stream']
    sites = {int(k): v for k, v in m['call_sites'][arm]['sites'].items()}
    with lzma.open(stream_path, 'rt') as stream:
        result = function_envelope(samples(stream), str(profile / m['arms'][arm]['path']),
            'tactix::vivify_apply(', {m['phase_entry']: None}, m['phase_caller'], sites)
    expected = prior['arms'][arm]
    assert result['samples'] == expected['samples'] and result['total_period'] == expected['total_period']
    for name, components in expected['components'].items():
        assert result['by_phase'].get(name, 0) == components.get('vivify', 0), (arm, name)
    assert not result['by_phase'].get('no_phase_frame', 0)
    assert not result['by_phase'].get('unresolved_phase_caller', 0)
    assert result['selected_period'] == sum(v.get('vivify', 0) for v in expected['components'].values()) > 0
    arms[arm] = result
    print(arm, 'exact complete Vivify sampled periods', result['selected_period'], flush=True)
for p in (Path(__file__), root / 'tools/bench/perf_function_envelope.py', root / 'tools/bench/test_perf_function_envelope.py'):
    hashes[str(p.relative_to(root))] = sha(p)
with target.open('x') as out:
    json.dump(dict(status='PASS', hashes=hashes, arms=arms,
        scope='Independent whole-function membership reproduces both frozen7132 wave0 Vivify component totals, including initial and search phases; no extra profile, solver run, expansion or deletion. This is reader validation, not new performance evidence.'), out, indent=2)
