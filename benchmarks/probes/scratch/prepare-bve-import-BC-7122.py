#!/usr/bin/env python3
"""Preserve B28+C5 membership/resources; pair approved stage2 with7087."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
dependencies = [
    'benchmarks/runs/20260910-7103-bve-views-BC-gate/neutrality-7104.json',
    'benchmarks/runs/20260910-7110-bve-import-build/focused/summary.json',
    'benchmarks/runs/20260910-7113-bve-import-proofs/summary.json',
    'benchmarks/runs/20260910-7114-bve-import-sanitizers/summary.json',
    'benchmarks/runs/20260910-7115-bve-import-sweep-proofs/summary.json',
    'benchmarks/runs/20260910-7118-bve-import-bounds-sanitizers/summary.json',
    'benchmarks/runs/20260910-7119-bve-import-bounds-proofs/summary.json',
    'benchmarks/runs/20260910-7120-bve-import-bounds-sweep-proofs/summary.json',
    'benchmarks/runs/20260910-7121-bve-import-bounds-fixtures/summary.json',
]
for rel in dependencies:
    g = json.loads((root / rel).read_text())
    for path, digest in g.get('hashes', {}).items(): assert sha(root / path) == digest, path
    if 'status' in g: assert g['status'] == 'PASS'
    if 'checks' in g and g['checks'] and isinstance(g['checks'][0], dict):
        assert all(c['exit_code'] == 0 for c in g['checks'])
    if 'checked' in g:
        assert g['checked'] in (96, 120) and g['negative_controls'] == g['checked'] + 2
        assert g['exact_parent_certificates']
gate = json.loads((root / dependencies[0]).read_text())
assert gate['solver_records'] == 66 and gate['all_work_and_certificates_equal']
source = Path(__file__).with_name('prepare-material-schedule-BC-6584.py')
code = source.read_text()
changes = {
    '20260909-6427-activation-stamp-BC-gate': '20260910-7103-bve-views-BC-gate',
    '20260909-6424-activation-stamp-build': '20260910-7087-bve-views-build',
    '20260909-6579-material-schedule-build': '20260910-7116-bve-import-bounds-build',
    '20260909-6584-material-schedule-BC-gate': '20260910-7122-bve-import-BC-gate',
    '20260909-6582-material-proof-control': '20260910-7119-bve-import-bounds-proofs',
    "'6424-activation-stamp'": "'7087-bve-views'",
    "'6579-material-schedule'": "'7116-bve-import-bounds'",
    "'6114-substitute-publication-BC', '6584-material-schedule-BC'": "'7103-bve-views-BC', '7122-bve-import-BC'",
    "root / 'tools/bench/runtime_task.py'": "old / 'pins/runtime_task.py'",
    'Approved canonical material-flag and Eliminate watermark cutover B28+C5 gate against immutable6424. Stage-one neutral control6574 is a dependency.':
        'Approved BVE mapping/index stage2 against immutable7087; unchanged full B28 and predeclared C5, frozen runner/resource contract.7110 preserved;7116 adds bounded-consumer and unit-fixture coverage.',
    'New flags and the material counter recover missing native invitations and retain pending canonical work. Require all expected verdicts and independent proofs/models, no solve loss or unexplained termination; timing is diagnostic, not acceptance.':
        'Fuse BVE physical-clause mapping with SubSolver import, remove duplicate touched-list and defer XOR-only indexes. Require exact cross-arm reported work/certificates. No strategy, budget, proof vocabulary or public API change.',
    'Changed work/certificates are expected, not a neutrality requirement.':
        'Exact cross-arm work/certificates required; differences fail the cost-only premise.',
    "dependencies=['benchmarks/runs/20260909-6574-material-state-BC-gate/neutrality-6576.json', 'benchmarks/runs/20260909-6582-material-proof-control/summary.json']":
        'dependencies=' + repr(dependencies),
}
# Simultaneous substitutions preserve replacements which themselves mention
# the old proof suffix used elsewhere in this frozen template.
ordered = sorted(changes.items(), key=lambda item: len(item[0]), reverse=True)
for i, (before, after) in enumerate(ordered):
    assert before in code, before
    code = code.replace(before, f'__IMPORT_{i}__')
for i, (_, after) in enumerate(ordered): code = code.replace(f'__IMPORT_{i}__', after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
