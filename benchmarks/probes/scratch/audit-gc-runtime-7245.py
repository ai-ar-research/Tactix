#!/usr/bin/env python3
"""Freeze broad solver-gate completion and the independent GC runtime launch."""
from pathlib import Path
base = Path(__file__).with_name('audit-gc-forward-7239.py')
code = base.read_text()
for before,after in {
    "target = root / 'benchmarks/reports/2026-09-11-gc-forward-audit-7239.json'":
        "target = root / 'benchmarks/reports/2026-09-11-gc-runtime-audit-7245.json'",
    "old_path = root / 'benchmarks/reports/2026-09-11-and-collection-audit-7232.json'":
        "old_path = root / 'benchmarks/reports/2026-09-11-gc-forward-audit-7239.json'",
    "for name in helpers: bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)":
        "helpers += ('prepare-gc-forward-runtime-7240','check-submit-gc-forward-runtime-7240','prepare-gc-forward-profiles-7241','validate-gc-forward-profiles-7241','submit-gc-forward-profile-wave-7241','extract-gc-forward-wave-7242','localize-gc-forward-collection-7243','assess-root-binary-7244','audit-gc-runtime-7245')\nfor name in helpers: bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)",
    "deleted = proofs['removed_bytes'] + locks['removed_bytes']":"deleted = 0",
    "r = bound_json(p, own=True); deleted += r['temporary_expansions_deleted_bytes']":
        "r = bound_json(p, own=True)\n    if wave == 2: deleted += r['temporary_expansions_deleted_bytes']",
    '2026-09-11-gc-forward-checkpoint-7239.md':'2026-09-11-gc-runtime-checkpoint-7245.md',
    "('20260911-7238-gc-forward-BC',66)]":"('20260911-7238-gc-forward-BC',66), ('20260911-7240-gc-forward-runtime',18), ('20260911-7241-gc-forward-profiles',6)]",
    "assert campaigns[specs[0][0]]['checked'] == 50":
        "assert campaigns[specs[0][0]]['checked'] == 50\nassert campaigns['20260911-7238-gc-forward-BC']['published'] == 66\nassert campaigns['20260911-7221-root-binary-BC']['checked'] == 66",
    'report-to-andrew/20260911-0214-gc-forward':'report-to-andrew/20260911-0230-gc-runtime',
    "'localize-root-binary-collection-7236','check-submit-root-binary-runtime-7226')":
        "'localize-root-binary-collection-7236','check-submit-root-binary-runtime-7226','localize-gc-forward-collection-7243','check-submit-gc-forward-runtime-7240','assess-root-binary-7244')",
    "'validate-root-binary-profiles-7230','check-submit-gc-forward-BC-7238')]":
        "'validate-root-binary-profiles-7230','check-submit-gc-forward-BC-7238','check-submit-gc-forward-runtime-7240','validate-gc-forward-profiles-7241')]",
    "commands = [[sys.executable":
        "assessment = bound_json(root / 'benchmarks/reports/2026-09-11-root-binary-assessment-7244.json', own=True)\nassert assessment['runtime_expected'] == 18 and assessment['profile_expected'] == 6\ncommands = [[sys.executable",
    'Read-only bookkeeping recheck confirmed prior evidence without changing next action; revalidated here and resumed concrete implementation rather than another status-only turn.':
        'Previous turn made concrete progress: isolated7233 implemented, full gates passed,7238 submitted and coherent checkpointcd7953d8 committed. This turn closes all66 BC solver outcomes, launches7240 runtime and prepares reviewed profile configuration without waiting for proofs.',
    '7233 fused sparse filter/forward is independently based on retained7144.117/117/116 suites,26240 watch comparisons,896 actual parent collector states,12 sanitizer runs and232 checked fixed UNSAT proofs pass.7238 BC launched; no performance claim or promotion. Continue predeclared root-binary waves without proof wait.':
        '7238 all66 expected/33 exact solver pairs enable7240 independent runtime waves. Full7233/7234/7235/7237 gates remain bound and unchanged.7241 prepared and wave0 submitted after exact Rank35 pair; reviewed calls507cf3/5085a3/509142 plus unchanged7162 control.7221root-binary66checked-clean;7244 completes all7226/7230 repetitions and preserves every ratio/RSS. No phase-only promotion.',
    'Only hash-verified owned profile expansions and consumed fixed micro-test inputs/proofs removed. Compressed profiles, extracted streams, active campaign scratch and prior failed fixtures preserved.':
        'Prior7239 owns fixed-proof and first-two-wave deletion receipts. This checkpoint records only any newly verified third root-profile expansion; otherwise zero new deletions. Compressed profiles, extracted streams, active campaign scratch and prior failed fixtures preserved.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
