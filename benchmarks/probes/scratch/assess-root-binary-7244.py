#!/usr/bin/env python3
"""Retain all root-binary runtime/RSS pairs and complete Reduce profiles."""
from pathlib import Path
base = Path(__file__).with_name('assess-factor-arena-7192.py')
code = base.read_text()
for before,after in {
    '2026-09-11-factor-arena-assessment-7192.json':'2026-09-11-root-binary-assessment-7244.json',
    '20260910-7186-factor-arena-runtime':'20260911-7226-root-binary-runtime',
    '20260910-7188-factor-arena-samples':'20260911-7231-root-binary-samples',
    'check-submit-factor-arena-runtime-7186.py':'check-submit-root-binary-runtime-7226.py',
    'validate-factor-arena-profiles-7187.py':'validate-root-binary-profiles-7230.py',
    'factor_ratios':'reduce_ratios',
    'complete_factor':'complete_reduce',
    'full Factor envelope':'full Reduce envelope',
    'Complete Factor':'Complete Reduce',
    'Arena assessment:':'Root-binary assessment:',
    'Decide whether the measured net gain supports retaining the isolated prototype and a fresh all50 transfer. Keep larger Vivify/Sweep/Congruence and31/43 search-work gaps in priority; do not keep iterating tiny Factor changes by default.':
        'Judge the root-binary prototype by all three whole-runtime and RSS distributions, not only complete Reduce savings. Preserve adverse31/35 results and31 memory cost; no favorable-wave selection or alignment tuning. The independent7233 fused GC experiment remains on retained7144, not a composition. No full50 transfer, promotion or fuzz acceptance from a local-only reduction.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
