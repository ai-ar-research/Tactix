#!/usr/bin/env python3
"""Freeze the completed shrink rejection and live binary timing handoff."""
from pathlib import Path
source = Path(__file__).with_name('audit-subsolver-cost-6885.py')
code = source.read_text()
for before,after in {
    '2026-09-10-subsolver-cost-audit-6885.json':'2026-09-10-subsolver-timing-audit-6891.json',
    '2026-09-10-sweep-rootfree-audit-6854.json':'2026-09-10-subsolver-cost-audit-6885.json',
    "'tools/slurm/wait.py','tools/slurm/test_wait.py'":"'tools/bench/inprocessing_cost.py','tools/bench/test_inprocessing_cost.py'",
    'benchmarks/reports/2026-09-10-subsolver-cost.md':'benchmarks/reports/2026-09-10-subsolver-timing.md',
    'range(6856,6887)':'range(6887,6893)',
    '20260910-6856-subsolver-propagation-annotation':'20260910-6888-vivify-propagation-annotation',
    '724221844':'557169956',
    "for rel in (\n    '20260910-6850":
        "summary(root/'benchmarks/runs/20260910-6888-vivify-propagation-annotation/location-6889.json')\nfor rel in (\n    '20260910-6864-subsolver-shrink-runtime/solver-checkpoint-6866.json',\n    '20260910-6864-subsolver-shrink-runtime/timer-cost-6887.json',\n    '20260910-6850",
    "('20260910-6864-subsolver-shrink-runtime',18,6)":"('20260910-6864-subsolver-shrink-runtime',18,18)",
    "('20260910-6873-subsolver-binary-BC-gate',66,0)":"('20260910-6873-subsolver-binary-BC-gate',66,66)",
    "('20260910-6882-subsolver-binary-runtime',18,0)":"('20260910-6882-subsolver-binary-runtime',18,6)",
    'report-to-andrew/20260910-0208-subsolver-cost':'report-to-andrew/20260910-0225-subsolver-timing',
    'fixed_regenerable_input_proof_bytes=removed':'fixed_regenerable_input_proof_bytes=0',
    'Only owning fixed-check/annotation helpers removed exact temporary products. Compressed originals, failed controls, all live scratch and prior authorities retained. Checked published outcomes clean. No fuzz artifact access.':
        'Only annotation6888 removed its557169956-byte temporary expansion this turn; prior fixed-proof deletions remain owned by previous helpers, not newly counted. Compressed originals, raw annotations, failures, all live scratch and prior authorities retained. Published checked outcomes clean. No fuzz artifact access.',
    '6730 current/unpromoted/unfuzzed, full50 target unmet. User approved6869 prototype only;6869/6857 experimental,6838/6822 unadopted. No global scheduling/CDCL or memory parity. Continue exact solver gates with proofs async; any invalidity pauses dependents. No Rank21, FlatLists, promotion, denied transfer/push retry. Report local/unsent. Winddown05:45/cutoff06:00EDT September10.':
        '6730 current/unpromoted/unfuzzed, full50 target unmet.6857 shrink-only NOTadopted after all18 exact timings,39 adverse in allpairs;6838/6822 also unadopted. User-approved6869 still experimental, wave1live; finalwave2 onlyafter preceding12exactsolverrecords, proofsasync.6888/6889 read existing checked Vivify profile, no new solver source/representation or speedup claim. No global scheduling/CDCL or memory parity. No Rank21, FlatLists, promotion, denied transfer/push retry. Report local/unsent. Winddown05:45/cutoff06:00EDT September10.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
