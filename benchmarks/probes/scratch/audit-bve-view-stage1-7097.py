#!/usr/bin/env python3
"""Checkpoint audit: passing suites and explicit failed gates; no submissions."""
from pathlib import Path
source = Path(__file__).with_name('audit-implementation-cost-7080.py')
code = source.read_text()
start = code.index("owned = ['PROGRESS.md'")
end = code.index('def bind(', start)
code = code[:start] + """owned = ['PROGRESS.md', 'docs/implementation-notes.md',
         'benchmarks/reports/2026-09-10-bve-view-stage1-checkpoint.md']
owned += ['benchmarks/probes/scratch/' + name for name in (
    'prepare-bve-views-7087.py', 'build-bve-views-7088.py',
    'check-bve-view-proofs-7089.py', 'test-bve-view-proofs-7089.cpp',
    'sanitize-bve-views-7090.py', 'prepare-bve-views-BC-7091.py',
    'submit-bve-views-BC-7091.py', 'check-bve-view-proofs-7095.py',
    'test-bve-view-proofs-7095.cpp', 'claude-bve-view-review-7096.txt',
    'run-claude-bve-view-review-7096.py', 'audit-bve-view-stage1-7097.py')]
for name in ('20260910-7087-bve-views-build', '20260910-7089-bve-view-proofs',
             '20260910-7090-bve-view-sanitizers', '20260910-7095-bve-view-proofs'):
    campaign = root / 'benchmarks/runs' / name
    assert not (campaign / 'launch.json').exists()
    owned += [str(p.relative_to(root)) for p in campaign.rglob('*')
              if p.is_file() and p.suffix in ('.json', '.patch', '.log')]
assert not (root / 'benchmarks/runs/20260910-7091-bve-views-BC-gate').exists()
assert not (root / 'benchmarks/runs/20260910-7090-bve-view-sanitizers/summary.json').exists()
assert not (root / 'benchmarks/runs/20260910-7095-bve-view-proofs/summary.json').exists()
assert not (root / 'benchmarks/probes/scratch/claude-bve-view-review-7096-response.json').exists()
""" + code[end:]
changes = {
 '2026-09-10-implementation-cost-audit-7080.json': '2026-09-10-bve-view-stage1-audit-7097.json',
 "('20260910-7030-collect-retry-build', '20260909-6730-reduce-watermark-build')":
   "('20260910-7030-collect-retry-build', '20260909-6730-reduce-watermark-build', '20260910-7087-bve-views-build')",
 "for rel in ('benchmarks/reports/2026-09-10-rank35-cost-scopes-7079.json',\n            'benchmarks/runs/20260910-7073-rank31-bve-priority/solver-summary-7075.json'):":
   "for rel in ('benchmarks/runs/20260910-7089-bve-view-proofs/summary.json',\n            'benchmarks/runs/20260910-7073-rank31-bve-priority/solver-summary-7075.json'):",
 "for rel in ('kissat/src/resolve.c', 'kissat/src/eliminate.c',\n            '.claude/worktrees/runtime-collect-retry-7030/src/rules/bve/bve.cpp'):":
   "for rel in ('kissat/src/resolve.c', 'kissat/src/definition.c', 'cadical/src/elim.cpp',\n            '.claude/worktrees/runtime-bve-views-7087/src/rules/bve/antecedent.hpp'):",
 '20260910-1115-implementation-cost': '20260910-1320-bve-view-stage1',
 "'check-default-strategy-config', 'check-proof-rule-sync')":
   "'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')",
 'for rel in sorted(set(owned)): bind(root / rel)':
   """bind(Path(__file__).with_name('audit-implementation-cost-7080.py'))
bind(Path(__file__).with_name('build-substitute-retry-6233.py'))
failed_gates = {}
for rel in ('benchmarks/runs/20260910-7090-bve-view-sanitizers/2.log',
            'benchmarks/runs/20260910-7095-bve-view-proofs/logs/previous-0-1-0-0-0-check.json'):
    failed_gates[rel] = (root / rel).read_text()
    bind(root / rel)
assert 'assigned_count + k.unassigned == k.nvars' in next(iter(failed_gates.values()))
assert 'NOT VERIFIED' in list(failed_gates.values())[1]
for rel in sorted(set(owned)): bind(root / rel)""",
 'owned_paths=sorted(set(owned)), scheduler=scheduler, campaigns=campaigns, checks=checks,':
   'owned_paths=sorted(set(owned)), scheduler=scheduler, campaigns=campaigns, checks=checks, failed_gates=failed_gates,',
 'Status-only response was no progress. Current continuation validates7075 and derives7079 cost envelopes, changing the next action toward broad inprocessing implementation efficiency.':
   'User approved the structural prototype, resuming the previously blocked goal. This turn implements isolated7087 antecedent views and validates114/114/113 suites; stronger sanitizer/proof fixtures exposed failures, preserved explicitly rather than waived.',
 'No new jobs/source/defaults. Audit common-scope cost and scheduling strength before another leaf optimization.7077 remains preserved/unsubmitted. Runtime/memory/current-source fuzz goals unmet.':
   'Stage1 source changed only BVE representation and its private preparation helper/test. No source promotion/default/scheduling/kernel-layout change. Required bounded Claude fixture review7096 awaits separate external-transfer approval; no invocation or dependent benchmark submission. Runtime, supported sanitizer checks, strong proof controls and current-source fuzz remain unmet.',
 'No material manual deletion. Published checked/clean records attest their campaign residue; active checks are not accepted. No active scratch or fuzz artifact access. Prepared7077 is a preserved secondary diagnostic, not abandoned; no deletion attempted.':
   'No material deletion. All old campaign proofs now checked/clean. Preserve7089 fixed certificates and7090/7095 failed artifacts; no sanitizer success or strong-proof success inferred.7091 exists only as unexecuted helpers;7077 remains prepared/unsubmitted. No fuzz artifact access or denied transfer/push retry.'
}
for before, after in changes.items():
    assert code.count(before) == 1, (before, code.count(before))
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
