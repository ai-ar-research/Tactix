#!/usr/bin/env python3
"""User-requested full-fifty rerun of exact7144 before further integration."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
import fixed_runtime as f
from checked_comparison import classify

c = root / 'benchmarks/runs/20260910-7167-vivify-revalidation-ranks1-50'
assert not c.exists()
evidence = {}
def bind(p, expected=None):
    digest = f.sha(p)
    assert expected is None or digest == expected, p
    evidence[str(p.relative_to(root))] = digest

for name in ('7163-vivify-revalidation-controls', '7164-vivify-revalidation-sanitizers',
             '7165-vivify-revalidation-proofs'):
    p = root / f'benchmarks/runs/20260910-{name}/summary.json'
    g = json.loads(p.read_text())
    assert g['status'] == 'PASS'
    bind(p)
    for rel, digest in g['hashes'].items(): bind(root / rel, digest)

gate = root / 'benchmarks/runs/20260910-7166-vivify-revalidation-BC-gate'
bind(gate / 'manifest.json')
gm = json.loads((gate / 'manifest.json').read_text())
for rel, digest in gm['pin_sha256'].items(): bind(gate / rel, digest)
for task in range(66):
    p = gate / f'results/{task:03d}.json'
    final = p.exists()
    if not final: p = gate / f'provisional/{task:03d}.json'
    r = json.loads(p.read_text())
    state = classify(r, final)
    assert state['status'] in ('checked_SAT', 'checked_UNSAT', 'pending_validation'), (p, state)
    assert r['expected_match'] and not r['solve']['timed_out']
    bind(p)

for rel in ('benchmarks/runs/20260910-7150-vivify-compact-runtime/solver-checkpoint-7155.json',
            'benchmarks/reports/2026-09-10-vivify-compact-assessment-7158.json',
            'benchmarks/reports/2026-09-10-vivify-gain-explanation-7161.json',
            'benchmarks/runs/20260910-7053-collect-retry-ranks1-50/manifest.json',
            'benchmarks/reports/2026-09-10-collect-retry-checked50-7098/summary.json'):
    bind(root / rel)
timing = json.loads((root / 'benchmarks/runs/20260910-7150-vivify-compact-runtime/solver-checkpoint-7155.json').read_text())
assert timing['solver_outcomes'] == 18 and timing['exact_own_repeats'] and timing['exact_cross_arm_work']
authority = root / 'benchmarks/runs/20260910-7162-vivify-revalidation-build'
original = json.loads((root / 'benchmarks/runs/20260910-7144-vivify-compact-build/manifest.json').read_text())
for kind in ('release', 'bc', 'trace'):
    p = root / f'build/pin-7162-vivify-revalidation-{kind}/tactix'
    bind(p, original['binaries'][f'build/pin-7144-vivify-compact-{kind}/tactix'])
bind(Path(__file__))
f.prepare(c, authority, root / 'build/pin-7162-vivify-revalidation-release/tactix',
          root / 'build/pin-7162-vivify-revalidation-bc/tactix')
m = json.loads((c / 'manifest.json').read_text())
m.update(evidence_sha256=evidence,
    parent_comparison='benchmarks/runs/20260910-7053-collect-retry-ranks1-50',
    hypothesis='Revalidate user-retained7144 compact Vivify on every approved rank before further integration. Prior matched gains are real but small; preserve all solve losses, invalidities and RSS tradeoffs. No assumed full50 transfer.',
    decision='User requested rerun everything before further integration, then explicitly said not to wait for all proof checks. Fresh full suites, fixed controls/sanitizers/proofs, B28+C5 expected outcomes precede this launch; independent broad checks remain asynchronous. No new integration commit until solver-phase validation. Current-source fuzz remains unavailable, not waived.',
    validation=m['validation'] + ' Compare every available non-timing work/proof fingerprint to checked7030 and repeated7144 witnesses; classify any mismatch before scoring. Fixed Kissat reference remains unchanged.')
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
