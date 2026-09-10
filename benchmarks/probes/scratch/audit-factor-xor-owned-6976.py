#!/usr/bin/env python3
"""Bind both uncommitted prior audits explicitly before final landing."""
from pathlib import Path
source = Path(__file__).with_name('audit-factor-xor-landing-6973.py')
wrapper = source.read_text()
tail = "exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})"
assert wrapper.count(tail) == 1
scope = {'__file__':str(source)}
exec(compile(wrapper.replace(tail,'expanded_code = code'),str(source),'exec'),scope)
code = scope['expanded_code']
for before,after in {
    "target = root/'benchmarks/reports/2026-09-10-factor-xor-landing-audit-6973.json'":
        "target = root/'benchmarks/reports/2026-09-10-factor-xor-owned-audit-6976.json'",
    "prior_path = root/'benchmarks/reports/2026-09-10-factor-xor-cost-audit-6940.json'":
        "prior_path = root/'benchmarks/reports/2026-09-10-factor-xor-landing-audit-6973.json'",
    'range(6925,6975)':'range(6925,6978)',
    'evidence = {}':
        "owned.update({'benchmarks/reports/2026-09-10-factor-xor-cost-audit-6940.json','benchmarks/reports/2026-09-10-factor-xor-landing-audit-6973.json'})\nevidence = {}",
    "summary(ite/'landing-recovery-6972.json')":
        "summary(ite/'landing-recovery-6972.json')\nsummary(ite/'ownership-preflight-6975.json')",
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
