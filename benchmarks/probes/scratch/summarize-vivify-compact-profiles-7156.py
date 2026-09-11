#!/usr/bin/env python3
"""Preserve all three paired complete-Vivify observations and the15% gate."""
import json
from pathlib import Path
import statistics
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7153-vivify-compact-samples'
target = c / 'three-wave-summary-7156.json'
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
values = {arm: [r['function_envelopes'][arm]['selected_period'] for r in waves]
          for arm in ('candidate', 'previous')}
assert min(values['previous']) > 0 and min(values['candidate']) > 0
ratios = [a / b for a, b in zip(values['candidate'], values['previous'])]
assert ratios == [r['complete_vivify_ratio'] for r in waves]
phases = {}
for name in ('initial_probe', 'probe', 'eliminate', 'no_phase_frame'):
    parts = {arm: [r['arms'][arm]['phase_periods'].get(name, 0) for r in waves]
             for arm in ('candidate', 'previous')}
    assert min(parts['previous']) > 0
    phases[name] = dict(paired=describe([a / b for a, b in zip(parts['candidate'], parts['previous'])]),
                        arms={arm: describe(v) for arm, v in parts.items()})
hashes[str(Path(__file__).relative_to(root))] = sha(Path(__file__))
with target.open('x') as out:
    json.dump(dict(hashes=hashes, complete_vivify=dict(ratios=ratios, paired=describe(ratios),
        arms={arm: describe(v) for arm, v in values.items()}), phases=phases,
        vivify_fifteen_percent_sampled_target=statistics.median(ratios) <= .85,
        target='At least15% lower complete vivify_apply sampled cycles including initial/search phases and all descendants; all three independent pairs retained.',
        limitation='Not exact invocation time or solver RSS. Unprofiled7150/7155 owns whole-runtime and RSS gates; full proofs/cleanup, current-source fuzz, transfer and consolidation remain required before promotion.'), out, indent=2)
print(json.dumps(dict(complete_vivify=describe(ratios), phases=phases), indent=2))
