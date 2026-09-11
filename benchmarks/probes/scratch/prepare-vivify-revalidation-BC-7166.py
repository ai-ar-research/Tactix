#!/usr/bin/env python3
"""Prepare immutable compact-Vivify B28+C5 pairs against frozen7030."""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
dependencies = [
    'benchmarks/runs/20260910-7163-vivify-revalidation-controls/summary.json',
    'benchmarks/runs/20260910-7164-vivify-revalidation-sanitizers/summary.json',
    'benchmarks/runs/20260910-7165-vivify-revalidation-proofs/summary.json',
]
for rel in dependencies:
    g = json.loads((root / rel).read_text())
    assert g['status'] == 'PASS'
    for path, digest in g['hashes'].items(): assert sha(root / path) == digest, path
proofs = json.loads((root / dependencies[-1]).read_text())
assert proofs['checked'] == 96 and proofs['negative_controls'] == 192
assert proofs['exact_parent_certificates']
c = root / 'benchmarks/runs/20260910-7162-vivify-revalidation-build'
expected = 'vivify-compact-contract cases=1728 digest=11603ba2e1d600bb proof-bytes=39624 subsumed=1872\n'
for kind, tests in (('release', 115), ('bc', 115), ('trace', 114)):
    assert (c / f'logs/{kind}-test_vivify_compact_contract.log').read_text() == expected
    assert (c / f'logs/{kind}-test_vivify_order_contract.log').read_text() == 'vivify-order-contract cases=96 digest=c67723e59482dad7\n'
    assert f'100% tests passed out of {tests}' in (c / f'logs/{kind}-ctest.log').read_text()

template = Path(__file__).with_name('prepare-material-schedule-BC-6584.py')
code = template.read_text()
changes = {
    '20260909-6427-activation-stamp-BC-gate': '20260910-7122-bve-import-BC-gate',
    '20260909-6424-activation-stamp-build': '20260910-7030-collect-retry-build',
    '20260909-6579-material-schedule-build': '20260910-7162-vivify-revalidation-build',
    '20260909-6584-material-schedule-BC-gate': '20260910-7166-vivify-revalidation-BC-gate',
    '20260909-6582-material-proof-control': '20260910-7165-vivify-revalidation-proofs',
    "'6424-activation-stamp'": "'7030-collect-retry'",
    "'6579-material-schedule'": "'7162-vivify-revalidation'",
    "'6114-substitute-publication-BC', '6584-material-schedule-BC'": "'7122-bve-import-BC', '7166-vivify-revalidation-BC'",
    "root / 'tools/bench/runtime_task.py'": "old / 'pins/runtime_task.py'",
    'Approved canonical material-flag and Eliminate watermark cutover B28+C5 gate against immutable6424. Stage-one neutral control6574 is a dependency.':
        'Compact canonical Vivify representation pilot against frozen7030; unchanged full B28+C5 membership and runner/resource/checker contract.',
    'New flags and the material counter recover missing native invitations and retain pending canonical work. Require all expected verdicts and independent proofs/models, no solve loss or unexplained termination; timing is diagnostic, not acceptance.':
        'Remove literal snapshots and large candidate comparison records in homogeneous canonical schedules; use ordinary-irredundant stable radix and redundant arena/ref ordering. Exact work/proof neutrality required.',
    'Changed work/certificates are expected, not a neutrality requirement.':
        'Changed work/certificates fail the cost-only premise; do not score invalid outcomes.',
    "dependencies=['benchmarks/runs/20260909-6574-material-state-BC-gate/neutrality-6576.json', 'benchmarks/runs/20260909-6582-material-proof-control/summary.json']":
        'dependencies=' + repr(dependencies),
    "test_consideration=b['test_consideration']":
        "test_consideration='115/115/114 full suites;96 ordering and1728 transaction/model/digest cases; six ASan/UBSan/LSan fixtures twice;96 independently checked proofs and192 negatives. Frozen build prose says3456 transaction cases by arithmetic mistake; actual fixture and logs prove1728, not3456.'",
}
ordered = sorted(changes.items(), key=lambda item: len(item[0]), reverse=True)
for i, (before, after) in enumerate(ordered):
    assert before in code, before
    code = code.replace(before, f'__VIVIFY_{i}__')
for i, (_, after) in enumerate(ordered): code = code.replace(f'__VIVIFY_{i}__', after)
exec(compile(code, str(template), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
