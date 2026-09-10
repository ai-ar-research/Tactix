#!/usr/bin/env python3
"""Fresh audit after the raw emitted patch was correctly refused by Git."""
from pathlib import Path
source = Path(__file__).with_name('audit-factor-xor-cost-6940.py')
wrapper = source.read_text()
tail = "exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})"
assert wrapper.count(tail) == 1
scope = {'__file__':str(source)}
exec(compile(wrapper.replace(tail,'expanded_code = code'),str(source),'exec'),scope)
code = scope['expanded_code']
for before,after in {
    "target = root/'benchmarks/reports/2026-09-10-factor-xor-cost-audit-6940.json'":
        "target = root/'benchmarks/reports/2026-09-10-factor-xor-landing-audit-6973.json'",
    "prior_path = root/'benchmarks/reports/2026-09-10-watch-rebuild-timing-audit-6923.json'":
        "prior_path = root/'benchmarks/reports/2026-09-10-factor-xor-cost-audit-6940.json'",
    "owned = {'PROGRESS.md','benchmarks/reports/2026-09-10-factor-xor-cost.md'}":
        "owned = {'PROGRESS.md','benchmarks/reports/2026-09-10-factor-xor-cost.md','benchmarks/reports/2026-09-10-factor-xor-landing-recovery.md'}",
    'range(6925,6972)':'range(6925,6975)',
    "own(ite/'parent.json'); own(ite/'compile-scope.patch')":
        "own(ite/'parent.json'); own(ite/'compile-scope-6972.patch')\nsummary(ite/'landing-recovery-6972.json')",
    "campaigns['benchmarks/runs/20260910-'+name] = {}":
        "campaigns.setdefault('benchmarks/runs/20260910-'+name,{})",
    "    cleanup=dict(fixed_proof_bytes=608678,profile_expansion_bytes=240062556,":
        "    landing_recovery='6941 refused raw unified-diff context whitespace before commit; frozen raw retained/hash-bound locally,6972 exact zero-context replay stages instead. Fresh6974 accepts only existing audited staged subset. No source/result change or gate weakening.',\n    cleanup=dict(fixed_proof_bytes=608678,profile_expansion_bytes=240062556,",
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
