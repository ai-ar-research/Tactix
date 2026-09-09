#!/usr/bin/env python3
"""Matched clean-Release cost diagnosis after complete3M decision closure."""
from pathlib import Path
source = Path(__file__).with_name('prepare-rank4-profile-6166.py')
code = source.read_text()
for old, new in (
    ('20260908-6115-substitute-publication-ranks1-50', '20260909-6366-compact-stamp-ranks1-50'),
    ('20260909-6166-rank4-release-profile', '20260909-6372-rank32-release-profile'),
    ("r['rank'] == '4'", "r['rank'] == '32'"),
    ("current / 'results/003.json'", "current / 'results/031.json'"),
    ("fresh / 'results/007.json'", "fresh / 'results/063.json'"),
    ('tactix-rank4-release-profile-v1', 'tactix-rank32-release-profile-v1'),
    ('benchmarks/runs/20260909-6160-rank4-current-prefix/summary.json',
     'benchmarks/runs/20260909-6365-compact-stamp-rank32-prefix/summary.json'),
    ('Localize ordinary current Release rank4 execution cost after907045 matched decisions through650K conflicts.',
     'Localize current Release Rank32 cost after153176971 matched decisions through3M conflicts and full conflict/decision counts equal to native. Fixed reference is unchanged.'),
    ('Dominant search operation cost,not demonstrated scheduling divergence,explains the remaining small rank4 gap. Rank hotspots before a source-backed local improvement.',
     'Observed329.772s versus fixed274.456s with equal full conflict/decision counts motivates profiling. Locate dominant candidate operations; do not equate cross-solver propagation/function scopes or infer pure unit cost from counter equality.'),
    ('No solver,checker orsampling implementation edit. Reuse hash-verified protocol;assert exact two-arm matrix and baseline identities. Existing39benchmark and13watcher tests cover unchanged surrounding contracts.',
     'No solver/checker/sampling implementation edit. Reuse immutable validated profile protocol and63benchmark/13watcher tests; require exact two-arm matrix/input identities, full6357 local106/105 suite and66 expected BC outcomes. Pending checks allow provisional development.'),
):
    assert old in code, old
    code = code.replace(old, new)
marker = 'root = Path(__file__).resolve().parents[3]'
assert code.count(marker) == 1
code = code.replace(marker, marker+"\nimport sys\nsys.path.insert(0,str(root/'tools/bench'))\nfrom checked_comparison import classify")
old = "for arm, path in baseline_paths.items():\n    record = json.loads(path.read_text())\n    assert record['valid'] and record['verdict'] == 'UNSAT' and record['expected_match']\n    assert record['artifact_deleted'] and record['derived_artifacts_deleted'] and not record['cleanup_pending']\n    assert record['binary_sha256'] == sha(c / 'pins' / arm)"
new = """for arm, path in list(baseline_paths.items()):
    if not path.exists():
        path=path.parent.parent/'provisional'/path.name
        baseline_paths[arm]=path
    record=json.loads(path.read_text())
    assert classify(record,path.parent.name=='results')['status'] in ('pending_validation','checked_UNSAT')
    assert record['instance']==rows[0]['instance'] and record['expected']=='UNSAT'
    if path.parent.name=='results':assert record['binary_sha256']==sha(c/'pins'/arm)
closure=json.loads((root/'benchmarks/runs/20260909-6365-compact-stamp-rank32-prefix/summary.json').read_text())
assert closure['decisions']['complete_equal'] and closure['matching_decisions_from_start']==153176971
gate=root/'benchmarks/runs/20260909-6362-compact-stamp-BC-gate/solver-checkpoint-6364.json'
assert json.loads(gate.read_text())['solver_records']==66
"""
assert code.count(old) == 1
code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
