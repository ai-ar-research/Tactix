#!/usr/bin/env python3
"""Final control-table checkpoint; prototype failures/approval remain open."""
from pathlib import Path
source = Path(__file__).with_name('audit-implementation-cost-7080.py')
code = source.read_text()
start = code.index("owned = ['PROGRESS.md'")
end = code.index('def bind(', start)
code = code[:start] + """owned = ['PROGRESS.md',
    'benchmarks/probes/scratch/seal-collect-retry-full50-7098.py',
    'benchmarks/probes/scratch/audit-checked-control-7099.py']
table = root / 'benchmarks/reports/2026-09-10-collect-retry-checked50-7098'
owned += [str(p.relative_to(root)) for p in table.iterdir() if p.is_file()]
assert not (root / 'benchmarks/runs/20260910-7091-bve-views-BC-gate').exists()
assert not (root / 'benchmarks/probes/scratch/claude-bve-view-review-7096-response.json').exists()
""" + code[end:]
changes = {
 '2026-09-10-implementation-cost-audit-7080.json': '2026-09-10-checked-control-audit-7099.json',
 "('20260910-7030-collect-retry-build', '20260909-6730-reduce-watermark-build')":
   "('20260910-7030-collect-retry-build', '20260909-6730-reduce-watermark-build', '20260910-7087-bve-views-build')",
 "for rel in ('benchmarks/reports/2026-09-10-rank35-cost-scopes-7079.json',\n            'benchmarks/runs/20260910-7073-rank31-bve-priority/solver-summary-7075.json'):":
   "for rel in ('benchmarks/reports/2026-09-10-collect-retry-checked50-7098/summary.json',\n            'benchmarks/runs/20260910-7073-rank31-bve-priority/solver-summary-7075.json'):",
 '20260910-1115-implementation-cost': '20260910-1328-checked-control',
 "'check-default-strategy-config', 'check-proof-rule-sync')":
   "'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')",
 'for rel in sorted(set(owned)): bind(root / rel)':
   "bind(Path(__file__).with_name('audit-implementation-cost-7080.py'))\nfor rel in sorted(set(owned)): bind(root / rel)",
 'Status-only response was no progress. Current continuation validates7075 and derives7079 cost envelopes, changing the next action toward broad inprocessing implementation efficiency.':
   'Previous turn made progress with isolated7087 prototype and explicit failed validation gates. This turn independently seals the now-final7030 full50 comparison table, requiring all150 candidate/parent/reference final records checked and clean.',
 'No new jobs/source/defaults. Audit common-scope cost and scheduling strength before another leaf optimization.7077 remains preserved/unsubmitted. Runtime/memory/current-source fuzz goals unmet.':
   'Final control table7098 is complete; ratio1.1231765117053298 remains above target, no timing changed. BVE view prototype7087 still awaits bounded Claude fixture review7096 and its separate external-transfer approval; no dependent benchmark or review invocation. Failed7090/7095 remain failed, not waived; no promotion.',
 'No material manual deletion. Published checked/clean records attest their campaign residue; active checks are not accepted. No active scratch or fuzz artifact access. Prepared7077 is a preserved secondary diagnostic, not abandoned; no deletion attempted.':
   'No material deletion. All old campaign results checked/clean. Preserve failed prototype validation artifacts; no fuzz access, external review invocation or denied push/transfer retry.7077 preserved/unsubmitted;7091 helpers unexecuted.'
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
