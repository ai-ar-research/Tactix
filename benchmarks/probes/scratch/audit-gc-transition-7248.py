#!/usr/bin/env python3
"""Freeze independent GC progress and the conserving cross-rule cost census."""
from pathlib import Path
base = Path(__file__).with_name('audit-gc-forward-7239.py')
code = base.read_text()
for before, after in {
    "target = root / 'benchmarks/reports/2026-09-11-gc-forward-audit-7239.json'":
        "target = root / 'benchmarks/reports/2026-09-11-gc-transition-audit-7248.json'",
    "old_path = root / 'benchmarks/reports/2026-09-11-and-collection-audit-7232.json'":
        "old_path = root / 'benchmarks/reports/2026-09-11-gc-runtime-audit-7245.json'",
    "for name in helpers: bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)":
        "helpers += ('census-transition-cost-7246','localize-forward-envelope-7247','audit-gc-transition-7248')\nfor name in helpers: bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)",
    "deleted = proofs['removed_bytes'] + locks['removed_bytes']": "deleted = 0",
    '20260911-7231-root-binary-samples': '20260911-7242-gc-forward-samples',
    '2026-09-11-root-binary-collection-7236-wave': '2026-09-11-gc-forward-collection-7243-wave',
    '2026-09-11-gc-forward-checkpoint-7239.md': '2026-09-11-gc-transition-checkpoint-7248.md',
    "('20260911-7238-gc-forward-BC',66)]":
        "('20260911-7238-gc-forward-BC',66), ('20260911-7240-gc-forward-runtime',18), ('20260911-7241-gc-forward-profiles',6)]",
    'report-to-andrew/20260911-0214-gc-forward': 'report-to-andrew/20260911-0258-gc-transition-cost',
    "commands = [[sys.executable":
        "for name in ('transition-cost-7246','forward-envelope-7247'):\n    bound_json(root / f'benchmarks/reports/2026-09-11-{name}.json', own=True)\ncommands = [[sys.executable",
    "'localize-root-binary-collection-7236','check-submit-root-binary-runtime-7226')":
        "'localize-gc-forward-collection-7243','check-submit-gc-forward-runtime-7240','census-transition-cost-7246','localize-forward-envelope-7247')",
    "'validate-root-binary-profiles-7230','check-submit-gc-forward-BC-7238')]":
        "'validate-root-binary-profiles-7230','check-submit-gc-forward-BC-7238','check-submit-gc-forward-runtime-7240','validate-gc-forward-profiles-7241')]",
    'Read-only bookkeeping recheck confirmed prior evidence without changing next action; revalidated here and resumed concrete implementation rather than another status-only turn.':
        'Previous bookkeeping answer was read-only and did not change the next development action: no progress toward convergence. Revalidated exact live jobs and pins, advanced predeclared GC waves, extracted new matched profiles and completed decision-changing cross-rule cost partitioning this turn.',
    '7233 fused sparse filter/forward is independently based on retained7144.117/117/116 suites,26240 watch comparisons,896 actual parent collector states,12 sanitizer runs and232 checked fixed UNSAT proofs pass.7238 BC launched; no performance claim or promotion. Continue predeclared root-binary waves without proof wait.':
        'GC runtime wave2/job1680863 submitted after12 exact outcomes, profile waves1/2 jobs1680861/1680867 after preceding exact pairs, without proof wait.7242/7243 waves0/1 complete: Reduce ratios.932876463/.925341775 and GC.876022892/.866754055, not final median/net gain.7246 disjoint transition census and7247 complete forward scopes refine the prior7160 representation lead and expose unmatched collection responsibilities. Retain7144, no production edit or local-only promotion.',
    'Only hash-verified owned profile expansions and consumed fixed micro-test inputs/proofs removed. Compressed profiles, extracted streams, active campaign scratch and prior failed fixtures preserved.':
        'Only newly expanded7242 GC profile files removed after hash verification; original compressed profiles and compressed extracted streams retained. No active checker scratch, prior failures, or fuzz artifacts touched.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
