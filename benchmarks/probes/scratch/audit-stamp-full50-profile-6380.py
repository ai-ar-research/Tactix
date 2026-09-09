#!/usr/bin/env python3
"""Reuse the audited census checkpoint protocol for the new decision evidence."""
from pathlib import Path

source = Path(__file__).with_name('audit-replicas-census-6347.py')
code = source.read_text()
replacements = {
    '2026-09-09-replicas-census-audit-6347.json': '2026-09-09-stamp-full50-profile-audit-6380.json',
    '2026-09-09-collapse-clean-audit-6337.json': '2026-09-09-compact-stamp-audit-6369.json',
    '20260909-6305-substitute-collapse-build': '20260909-6357-compact-stamp-build',
    '2026-09-09-collapse-replicas-and-transitive-census.md': '2026-09-09-stamp-closure-full50-profile.md',
    'range(6340,6349)': 'range(6371,6382)',
    "'20260909-6345-rank32-selected-transitive')": "'20260909-6345-rank32-selected-transitive', '20260909-6362-compact-stamp-BC-gate', '20260909-6365-compact-stamp-rank32-prefix', '20260909-6366-compact-stamp-ranks1-50', '20260909-6372-rank32-release-profile', '20260909-6374-compact-stamp-three-wave-runtime', '20260909-6376-rank35-stamp-prefix')",
    "'6332-' in name": "any(n in name for n in ('6332-','6374-'))",
    "else 66 if 'BC-gate' in name else 50": "else 66 if 'BC-gate' in name else 2 if '6372-' in name else 50",
    'assert len(latest)==expected,(name,len(latest))': "assert (len(latest)<=expected if '6374-' in name else len(latest)==expected),(name,len(latest))",
    "'solver-checkpoint-6343.json'": "'solver-checkpoint-6343.json','solver-checkpoint-6364.json','solver-summary-6378.json'",
    '20260909-0530-replicas-and-census': '20260909-0644-closure-full50',
    'No manual deletion. Completed6340 runner-owned partial-proof/input cleanup verified; partial proof is not final acceptance. All live checker and selected6345 scratch untouched. Frozen pins/cpu-q/baselines unchanged.': 'No manual deletion. Completed6365/6376 runner-owned partial-proof/input cleanup verified; partial proof is not final acceptance. Live checker scratch untouched. Raw profile/trace files retained locally with hashes. Frozen pins/cpu-q/baselines unchanged.',
    '6342 passed exact full replay/framing but incorrectly equated conflict-filtered counts to one invocation; preserved6342 and corrected with6344 plus two regression tests. No solver/proof failure.': 'No new solver/checker failure or excluded measurement. PROGRESS patch failed atomically because hunks were not in file order; corrected by ordering hunks. Read-only filename discovery misses and head-cut broken pipe did not affect measurements. No new acceptance/parser semantics: existing benchmark and SLURM suites cover reused protocols; new snapshot assertions cover new authorities.',
    'Validate6345 selected-pair non-transitive6340 replay and partial proof before interpretation; add selected-event framing tests. All6332 solver waves exact; continue checker outcomes, not duplicate timing. Sourcefuzz/memory/newtracker approval/promotion remain open; no denied push/transfer retry or fuzz inspection.': 'Submit6374 wave2 after12 exact solver outcomes, without waiting for certificates; then summarize three waves. Consider private Vivify first-count cache with actual old/new size/order/proof tests, or Rank50 tracker only if approved. Rank35 adverse closure currently100K; full50 goal remains unmet. Sourcefuzz/memory/promotion open; no denied push/transfer retry or fuzz inspection.',
}
for old, new in replacements.items():
    assert old in code, old
    code = code.replace(old, new)
extra = '''
for relative in ('benchmarks/reports/2026-09-09-compact-stamp-full50-6371/summary.json',
                 'benchmarks/runs/20260909-6365-compact-stamp-rank32-prefix/summary.json',
                 'benchmarks/runs/20260909-6376-rank35-stamp-prefix/summary.json'):
    p=root/relative; snapshot=json.loads(p.read_text()); retain(p)
    for rel,digest in snapshot['hashes'].items(): assert sha(root/rel)==digest,rel
    if '6371' in relative:
        assert snapshot['score']['expected_solves']==50 and snapshot['score']['ratio']>1.2
        retain(p.with_name('comparison.tsv'))
    elif '6365' in relative:
        assert snapshot['matching_decisions_from_start']==153176971 and snapshot['decisions']['complete_equal']
    else:
        assert all(snapshot['prior_rephase_closure'].values())
        assert snapshot['current_previous_decisions']['complete_equal']
        assert snapshot['current_previous_decisions']['matching_rows']==40636523
c=root/'benchmarks/runs/20260909-6372-rank32-release-profile'
p=c/'solver-summary-6378.json'; snapshot=json.loads(p.read_text()); retain(p)
assert sha(c/'manifest.json')==snapshot['manifest_sha256']
for arm in snapshot['arms'].values(): assert sha(root/arm['record'])==arm['record_sha256']
for p in (c/'profiles').rglob('*'):
    if p.is_file(): local[str(p.relative_to(root))]=dict(bytes=p.stat().st_size,sha256=sha(p))
'''
assert code.count('commands=[(n,') == 1
code = code.replace('commands=[(n,', extra + '\ncommands=[(n,')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
