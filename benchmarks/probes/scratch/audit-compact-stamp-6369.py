#!/usr/bin/env python3
"""Audit the isolated repair, controls, protected campaigns and pending gates."""
from pathlib import Path
source = Path(__file__).with_name('audit-replicas-census-6347.py')
code = source.read_text()
for old, new in (
    ('2026-09-09-replicas-census-audit-6347.json', '2026-09-09-compact-stamp-audit-6369.json'),
    ('2026-09-09-collapse-clean-audit-6337.json', '2026-09-09-transitive-schedule-audit-6354.json'),
    ('2026-09-09-collapse-replicas-and-transitive-census.md', '2026-09-09-compacted-queue-stamp.md'),
    ('range(6340,6349)', 'range(6356,6371)'),
    ('20260909-0530-replicas-and-census', '20260909-0617-compact-stamp'),
    ("'20260909-6345-rank32-selected-transitive')",
     "'20260909-6345-rank32-selected-transitive','20260909-6352-rank32-transitive-schedule','20260909-6362-compact-stamp-BC-gate','20260909-6365-compact-stamp-rank32-prefix','20260909-6366-compact-stamp-ranks1-50')"),
    ("assert len(latest)==expected,(name,len(latest))",
     "assert len(latest)<=expected if '6366-' in name else len(latest)==expected,(name,len(latest))"),
    ("'solver-checkpoint-6343.json'", "'solver-checkpoint-6343.json','solver-checkpoint-6364.json'"),
):
    assert old in code, old
    code = code.replace(old, new)
marker = 'commands=[(n,'
assert code.count(marker) == 1
extra = """owned.update({'tools/bench/transitive_schedule.py','tools/bench/test_transitive_schedule.py',
    'docs/implementation-notes.md','docs/scheduling-parity-ledger.md',
    'benchmarks/probes/scratch/test-native-compact-stamp-6360.c'})
schedule=root/'benchmarks/runs/20260909-6352-rank32-transitive-schedule/summary.json'
ss=json.loads(schedule.read_text());assert ss['parent_replay']
assert ss['snapshots']=={'candidate':4,'kissat':4}
for comparison in ss['comparisons']:
    assert comparison['same_compact_membership']
    assert comparison['differing_input_fields']=={'stamp':2}
    assert all(comparison['sorted_under_recorded_keys'].values())
    assert comparison['nonzero_priorities']=={'candidate':0,'kissat':0}
for rel,digest in ss['hashes'].items():assert sha(root/rel)==digest,rel
retain(schedule)
build=root/'benchmarks/runs/20260909-6357-compact-stamp-build'
bs=json.loads((build/'manifest.json').read_text())
for rel,digest in bs['files'].items():assert sha(root/bs['source']/rel)==digest,rel
for rel,digest in bs['binaries'].items():assert sha(root/rel)==digest,rel
for rel,digest in bs['patches'].items():assert sha(build/rel)==digest,rel
for rel,digest in bs['logs'].items():assert sha(build/rel)==digest,rel
for folder in (build,root/'benchmarks/runs/20260909-6359-compact-stamp-parent-control',
        root/'benchmarks/runs/20260909-6360-native-compact-stamp',
        root/'benchmarks/runs/20260909-6361-compact-stamp-controls'):
    for q in folder.rglob('*'):
        if q.is_file() and q.suffix in ('.json','.patch','.log','.cpp','.c'):retain(q)
controls=root/'benchmarks/runs/20260909-6361-compact-stamp-controls'
cs=json.loads((controls/'summary.json').read_text())
for rel,digest in cs['artifacts'].items():assert sha(controls/rel)==digest,rel
assert 'observations=144 missing=144' in cs['records']['parent']['output']
assert 'disconnected_failures=36' in cs['records']['parent']['output']
assert 'PASS actual native compaction cases=12' in cs['records']['native']['output']
gate=root/'benchmarks/runs/20260909-6362-compact-stamp-BC-gate/solver-checkpoint-6364.json'
gs=json.loads(gate.read_text());assert gs['solver_records']==66 and gs['checked']==64
for rel,digest in gs['hashes'].items():assert sha(root/rel)==digest,rel
retain(gate)
"""
code = code.replace(marker, extra+marker)
code = code.replace('Completed6340 runner-owned partial-proof/input cleanup verified; partial proof is not final acceptance. All live checker and selected6345 scratch untouched.',
    'Completed6340/6345/6352 runner-owned partial-proof/input cleanup verified; partial proof is not final acceptance. All live checker and current6365/6366 scratch untouched.')
code = code.replace("failure_classification='6342 passed exact full replay/framing but incorrectly equated conflict-filtered counts to one invocation; preserved6342 and corrected with6344 plus two regression tests. No solver/proof failure.',",
    "failure_classification='6359 predecessor fixture exposed detached-index0 self-connection;6361 records36 failures and144 missing stamps without asserting old correctness.6360 native fixture compile-only failure requires solver pointer for IDX macro;6361 corrected local alias and actual native12PASS.6364 helper absent provisional hash field fixed before checkpoint creation, preserving immutable pin/matrix/final-hash validation. No unexplained candidate solver or proof failure.',")
code = code.replace("next='Validate6345 selected-pair non-transitive6340 replay and partial proof before interpretation; add selected-event framing tests. All6332 solver waves exact; continue checker outcomes, not duplicate timing. Sourcefuzz/memory/newtracker approval/promotion remain open; no denied push/transfer retry or fuzz inspection.'",
    "next='Current6357 isolated repair passes106Release twice105trace and B/C66expected. Validate6365 inherited1M hash then entire3M prefix with6368; relocalize earlier if inherited hash differs. Price6366 all50 versus fixed matched reference and inspect work/RSS; then required paired timing. Continue final checker outcomes. Sourcefuzz/memory/newtracker approval/promotion remain open; no denied push/transfer retry or fuzz inspection.'")
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
