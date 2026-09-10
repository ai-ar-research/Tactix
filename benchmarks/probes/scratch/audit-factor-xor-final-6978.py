#!/usr/bin/env python3
"""Correct6976's read-only wrapper preflight; preserve every frozen artifact."""
from pathlib import Path
source = Path(__file__).with_name('audit-factor-xor-landing-6973.py')
wrapper = source.read_text()
tail = "exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})"
assert wrapper.endswith(tail+'\n')
scope = {'__file__':str(source)}
# Replace only the terminal invocation, not the identical string declaration.
exec(compile(wrapper.rsplit(tail,1)[0]+'expanded_code = code\n',str(source),'exec'),scope)
code = scope['expanded_code']
for before,after in {
    "target = root/'benchmarks/reports/2026-09-10-factor-xor-landing-audit-6973.json'":
        "target = root/'benchmarks/reports/2026-09-10-factor-xor-final-audit-6978.json'",
    "prior_path = root/'benchmarks/reports/2026-09-10-factor-xor-cost-audit-6940.json'":
        "prior_path = root/'benchmarks/reports/2026-09-10-factor-xor-landing-audit-6973.json'",
    'range(6925,6975)':'range(6925,6980)',
    'evidence = {}':
        "owned.update({'benchmarks/reports/2026-09-10-factor-xor-cost-audit-6940.json','benchmarks/reports/2026-09-10-factor-xor-landing-audit-6973.json'})\nevidence = {}",
    "summary(ite/'landing-recovery-6972.json')":
        "summary(ite/'landing-recovery-6972.json')\nsummary(ite/'ownership-preflight-6975.json')",
    "    landing_recovery=":
        "    audit_preflight_classification='6976 wrapper scanner found both a declared tail string and the terminal invocation; refused before executing audit or writing artifacts.6978 replaces only the terminal invocation.6976 audit and6977 landing never executed beyond that preflight. Both earlier audits are explicitly owned;6975 classifies6974. Fresh6979 retains strict ownership/whitespace gates.',\n    landing_recovery=",
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
