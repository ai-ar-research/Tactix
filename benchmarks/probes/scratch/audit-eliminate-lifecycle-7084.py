#!/usr/bin/env python3
"""Successor audit: complete phase attribution and approval-gated prototype."""
from pathlib import Path
source = Path(__file__).with_name('audit-implementation-cost-7080.py')
code = source.read_text()
start = code.index("owned = ['PROGRESS.md'")
end = code.index('def bind(', start)
code = code[:start] + """owned = ['PROGRESS.md', 'tools/bench/perf_callstack.py',
         'tools/bench/test_perf_callstack.py',
         'benchmarks/reports/2026-09-10-eliminate-lifecycle-cost.md']
owned += ['benchmarks/probes/scratch/' + name + '.py' for name in (
    'extract-rank35-callstacks-7081', 'summarize-rank35-lifecycles-7082',
    'audit-eliminate-lifecycle-7084')]
cost = root / 'benchmarks/runs/20260910-7081-rank35-callstack-cost'
assert not list(cost.glob('expanded-*')) and not (cost / 'launch.json').exists()
owned += [str(p.relative_to(root)) for p in cost.iterdir()
          if p.is_file() and p.suffix in ('.json', '.xz', '.stderr')]
""" + code[end:]
changes = {
    '2026-09-10-implementation-cost-audit-7080.json': '2026-09-10-eliminate-lifecycle-audit-7084.json',
    "for rel in ('benchmarks/reports/2026-09-10-rank35-cost-scopes-7079.json',\n            'benchmarks/runs/20260910-7073-rank31-bve-priority/solver-summary-7075.json'):":
        "for rel in ('benchmarks/runs/20260910-7081-rank35-callstack-cost/extraction-7081.json',\n            'benchmarks/runs/20260910-7081-rank35-callstack-cost/lifecycle-summary-7082.json'):",
    "for rel in ('kissat/src/resolve.c', 'kissat/src/eliminate.c',\n            '.claude/worktrees/runtime-collect-retry-7030/src/rules/bve/bve.cpp'):":
        "for rel in ('kissat/src/resolve.c', 'kissat/src/eliminate.c', 'kissat/src/definition.c',\n            'kissat/src/gates.c', 'cadical/src/elim.cpp', 'cadical/src/gates.cpp',\n            'docs/calculus.md', 'docs/implementation-notes.md',\n            '.claude/worktrees/runtime-collect-retry-7030/src/rules/bve/bve.cpp'):",
    '20260910-1115-implementation-cost': '20260910-1130-eliminate-lifecycle',
    'for rel in sorted(set(owned)): bind(root / rel)':
        "bind(Path(__file__).with_name('audit-implementation-cost-7080.py'))\nfor rel in sorted(set(owned)): bind(root / rel)",
    'Status-only response was no progress. Current continuation validates7075 and derives7079 cost envelopes, changing the next action toward broad inprocessing implementation efficiency.':
        'Previous turn was progress: a33c714d validates7075 and derives7079. This turn validates7081/7082 all-sample callsite attribution and distinguishes whole Eliminate/Probe costs, then requests approval for a structural BVE data-preparation prototype.',
    'No new jobs/source/defaults. Audit common-scope cost and scheduling strength before another leaf optimization.7077 remains preserved/unsubmitted. Runtime/memory/current-source fuzz goals unmet.':
        'No new jobs/source/defaults. Complete Rank35 Eliminate is119.296/55.643 billion sampled cycles. Structural BVE prototype approval requested, not received at this checkpoint; no edits until approved. Exact work/proof and public API required; no FlatLists.7077 remains prepared/unsubmitted. Runtime/memory/current-source fuzz goals unmet.',
    'No material manual deletion. Published checked/clean records attest their campaign residue; active checks are not accepted. No active scratch or fuzz artifact access. Prepared7077 is a preserved secondary diagnostic, not abandoned; no deletion attempted.':
        'Only1113682932 bytes of this offline helper newly created hash-verified perf expansions removed; originals and new compressed sample streams retained. No expanded-* residue remains in7081. Published checked/clean records attest campaign residue; active checks not accepted. No active scratch or fuzz access.7077 preserved/unsubmitted. The unrelated live bash allocation remains untouched.',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
