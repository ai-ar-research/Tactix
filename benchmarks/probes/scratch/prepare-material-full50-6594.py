#!/usr/bin/env python3
"""All50 sensitivity/memory gate; fixed Kissat authority stays unchanged."""
import json, sys
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import fixed_runtime as f
from checked_comparison import classify
gate=root/'benchmarks/runs/20260909-6584-material-schedule-BC-gate/summary-6586.json'
closure=root/'benchmarks/runs/20260909-6587-rank50-material-schedule/summary-6589.json'
g=json.loads(gate.read_text()); w=json.loads(closure.read_text())
assert g['solver_records']==66 and len(g['pairs'])==33
assert w['observer_neutral'] and w['decision_difference']['complete_equal']
assert w['decision_difference']['matching_rows']==1459556 and w['boundary_comparison']['equal']
for rel,digest in w['hashes'].items(): assert f.sha(root/rel)==digest,rel
# Revisit published validation before any dependent submission; pending proofs
# are authorized provisional passes, not a checked acceptance claim.
for path in (gate.parent/'results').glob('*.json'):
    status=classify(json.loads(path.read_text()),True)
    assert status['checked'] and status['clean'],(path,status)
c=root/'benchmarks/runs/20260909-6594-material-ranks1-50'
f.prepare(c,root/'benchmarks/runs/20260909-6579-material-schedule-build',
    root/'build/pin-6579-material-schedule-release/tactix',root/'build/pin-6579-material-schedule-bc/tactix')
m=json.loads((c/'manifest.json').read_text())
m.update(provisional_gate=dict(path=str(gate.relative_to(root)),sha256=f.sha(gate),expected=66,checked=g['checked_clean']),
    causal_closures={'50':dict(path=str(closure.relative_to(root)),sha256=f.sha(closure))},
    paired_timing=dict(path='benchmarks/runs/20260909-6592-rank50-material-three-wave-runtime/manifest.json',
        sha256=f.sha(root/'benchmarks/runs/20260909-6592-rank50-material-three-wave-runtime/manifest.json'),status='Three paired waves predeclared; not completed or accepted. Full50 collection proceeds independently, not in place of repetition.'),
    hypothesis='Canonical subsumption-material lifecycle restores Rank50 invitation and search prefix. Measure every rank, including adverse changes and memory, without assuming aggregate benefit.',
    decision='All50 fixed matched12CPU references and discrepancies retained. Independent checks, repeated timing and memory remain acceptance requirements; unavailable Claude-owned source fuzz blocks promotion. No source change, reference refresh, new tier selection or production mutation.',
    parent_comparison='benchmarks/runs/20260909-6439-activation-ranks1-50')
# Complete preparation metadata before freezing/submitting this new campaign.
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(c)
