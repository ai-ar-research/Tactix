#!/usr/bin/env python3
"""Predeclare matched repeats of the three largest full50 transfer regressions."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-runtime-7186.py')
code = base.read_text()
changes = {
    '20260910-7186-factor-arena-runtime': '20260911-7212-factor-arena-transfer-controls',
    '7186-factor-arena-runtime': '7212-factor-arena-transfer',
    'for rank in (31, 35, 38):': 'for rank in (8, 10, 13):',
    "r['verdict'] == r['expected'] == 'UNSAT'": "r['verdict'] == r['expected'] == 'SAT'",
    "expected='UNSAT'": "expected='SAT'",
    'tactix-factor-arena-runtime-v1': 'tactix-factor-arena-transfer-controls-v1',
    'Three independent paired31/35/38 six-task waves': 'Three independent paired8/10/13 six-task waves',
    'hypothesis=gm[\'hypothesis\']':
        "hypothesis='Frozen7201 all50 exact; aggregate31.800s slower, three largest regressions8/10/13 total78.331s with elevated page faults. Distinguish repeatable binary effects from cross-window/system effects without exclusions or changing the fixed native vector.'",
    'Frozen7150 exact fingerprint fixtures plus new7186 matrix/identity controls; underlying arena unit/proof/sanitizer gates bound through7183. No changes to solver or copied runtime runner.':
        'Frozen7150 exact fingerprints,7186 identity controls and classified SAT/UNSAT reader; no solver/runner changes. Selection computed from all50 deltas, not chosen for favorable results. Underlying arena gates bound through7183.',
    'Measure whole runtime and memory including arena holes. Complete Factor profiles are also required for structural unit-cost attribution. Normal telemetry retained. No production integration, default promotion or fuzz waiver.':
        'Report every paired time/RSS and hardware-counter context; compare full spreads before causal interpretation. Full7201 score is frozen and no outlier is discarded. Page-fault correlation is not a proven cause; no THP/NUMA/affinity/partition change. Structural7186/7187 Factor evidence remains separate. No production integration or fuzz waiver.',
    "bind(gate / 'manifest.json'); bind(gate / 'launch.json'); bind(reader); bind(Path(__file__))":
        """bind(gate / 'manifest.json'); bind(gate / 'launch.json'); bind(reader); bind(Path(__file__))
transfer = root / 'benchmarks/reports/2026-09-11-factor-arena-full50-7201/summary.json'
ts = json.loads(transfer.read_text()); bind(transfer)
assert ts['exact_parent_work_and_proofs'] and len(ts['rows']) == 50
top = sorted(ts['rows'], key=lambda r: r['candidate_seconds']-r['parent_seconds'], reverse=True)[:3]
assert sorted(r['rank'] for r in top) == [8, 10, 13]
for rel, digest in ts['hashes'].items(): bind(root / rel, digest)""",
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
