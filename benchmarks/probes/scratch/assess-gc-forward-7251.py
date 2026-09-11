#!/usr/bin/env python3
"""Assess every GC runtime/RSS repeat against complete Reduce cost."""
from pathlib import Path
base = Path(__file__).with_name('assess-root-binary-7244.py')
code = base.read_text()
for before, after in {
    '2026-09-11-root-binary-assessment-7244.json': '2026-09-11-gc-forward-assessment-7251.json',
    '20260911-7226-root-binary-runtime': '20260911-7240-gc-forward-runtime',
    '20260911-7231-root-binary-samples': '20260911-7242-gc-forward-samples',
    'check-submit-root-binary-runtime-7226.py': 'check-submit-gc-forward-runtime-7240.py',
    'validate-root-binary-profiles-7230.py': 'validate-gc-forward-profiles-7241.py',
    'Root-binary assessment:': 'GC-forward assessment:',
    'Judge the root-binary prototype by all three whole-runtime and RSS distributions, not only complete Reduce savings. Preserve adverse31/35 results and31 memory cost; no favorable-wave selection or alignment tuning. The independent7233 fused GC experiment remains on retained7144, not a composition. No full50 transfer, promotion or fuzz acceptance from a local-only reduction.':
        'Judge7233 by all whole-runtime/RSS repeats, not only nested GC/Reduce savings. Preserve every adverse control, no favorable-wave selection or alignment tuning. Continue the independent single-occurrence lifecycle redesign from retained7144. No full50 or production promotion from a local-only saving.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
