#!/usr/bin/env python3
"""Strict audited-subset landing including the two retained prior audits."""
from pathlib import Path
source = Path(__file__).with_name('land-factor-xor-recovery-6974.py')
code = source.read_text()
before = '2026-09-10-factor-xor-landing-audit-6973.json'
assert before in code
code = code.replace(before,'2026-09-10-factor-xor-owned-audit-6976.json')
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
