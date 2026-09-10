#!/usr/bin/env python3
"""Report all predeclared complete-Eliminate profile ratios, never best-of."""
import json
from pathlib import Path
import statistics
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7132-bve-lifecycle-samples'
target = c / 'three-wave-summary-7133.json'
assert not target.exists()
waves, hashes = [], {}
for wave in range(3):
    p = c / f'wave{wave}/summary.json'
    if not p.exists():
        print(f'Only {len(waves)}/3 extracted pairs; no summary written'); sys.exit(0)
    r = json.loads(p.read_text()); assert r['wave'] == wave
    hashes[str(p.relative_to(root))] = sha(p)
    for rel, digest in r['hashes'].items(): assert sha(root / rel) == digest, rel
    waves.append(r)
def describe(values):
    med = statistics.median(values)
    return dict(median=med, min=min(values), max=max(values),
                range_over_median=(max(values) - min(values)) / med if med else None)
phases = {}
for name in ('initial_probe', 'probe', 'eliminate', 'no_phase_frame'):
    values = {arm: [r['arms'][arm]['phase_periods'].get(name, 0) for r in waves]
              for arm in ('candidate', 'previous')}
    assert min(values['previous']) > 0
    ratios = [a / b for a, b in zip(values['candidate'], values['previous'])]
    phases[name] = dict(paired=describe(ratios), ratios=ratios,
        arms={arm: describe(v) for arm, v in values.items()})
total = {arm: [r['arms'][arm]['total_period'] for r in waves] for arm in ('candidate', 'previous')}
hashes[str(Path(__file__).relative_to(root))] = sha(Path(__file__))
with target.open('x') as stream:
    json.dump(dict(hashes=hashes, phases=phases,
        total_period={arm: describe(values) for arm, values in total.items()},
        eliminate_twenty_percent_sampled_target=phases['eliminate']['paired']['median'] <= .8,
        target='Approved20% complete-Eliminate Rank35 reduction; retain this threshold and all three independent paired observations.',
        limitation='Sampled cycle ratios, not exact per-operation latency. Check7130 final proof/cleanup states separately.7127 owns unprofiled whole-runtime/RSS gates; neither a sampled target nor a passing correctness gate by itself authorizes promotion. Native comparison remains the frozen7082 diagnostic, not a new Kissat baseline.'), stream, indent=2)
print(json.dumps({name: value['paired'] for name, value in phases.items()}, indent=2))
